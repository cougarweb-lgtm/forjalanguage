/* teste_elf_membros.c - censo de leitura dos membros ELF das 4 bibliotecas estaticas do Linux (third\linux\x86_64: libz, libsqlite3, libcjson, libcgltf).
 * Cada membro passa pelo leitor ELF (ElfLerObjeto) sem recusa; as secoes vivas somam por classe (OS_*) o que tests\nativo\linux\elf_ler\censo_secoes.txt
 * registra, e toda secao viva tem saida no ligador ELF (LigarSaidaElf). A saida nao depende do nome da secao: .rodata e .data.rel.ro antes davam X0903.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - PREPARO 0.24.1: a cola zlib_ponte entrou no libz.a (+1 membro ELF, o zlib_ponte.o): censo 19/28/45/19/19 (era 18/27/44/18/18).
 *            0.2.0 - M5c-b P61: OS_TDATA/OS_TBSS vao para LIGAR_SAIDA_TLS; TLS sem musl = X0113; forma GOTTPOFF (LigarElfFormaTls) com e sem REX.R.
 *            0.1.0 - M5b P37d: primeira versao. ElfMembrosVerificacoes (chamada pelo RodarUnidade do teste_nativo); caminhos das libs relativos a language\compiler.
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/diag.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/elf_ler.h"
#include "nativo/obj/ligar_elf.h"
#include "nativo/obj/ligar_estado.h"
#include "nativo/obj/objeto.h"
#include "cache/entradas_ler.h"

#define ELF_MEMBROS_QTD_LIBS 4
/* Totais do censo (censo_secoes.txt, 4 .a): 19 membros ELF; secoes vivas por classe (as descartadas nao entram no Objeto). */
#define ELF_MEMBROS_ESPERADO_MEMBROS 19u
#define ELF_MEMBROS_ESPERADO_TEXTO 28u
#define ELF_MEMBROS_ESPERADO_RDATA 45u
#define ELF_MEMBROS_ESPERADO_DATA 19u
#define ELF_MEMBROS_ESPERADO_BSS 19u

typedef struct {
    uint32_t membros, erros, texto, rdata, data, bss, outros, sem_saida;
} ElfMembrosContagem;

static const char *const ELF_MEMBROS_LIBS[ELF_MEMBROS_QTD_LIBS] = {
    "../third/linux/x86_64/zlib/libz.a",
    "../third/linux/x86_64/sqlite3/libsqlite3.a",
    "../third/linux/x86_64/cjson/libcjson.a",
    "../third/linux/x86_64/cgltf/libcgltf.a",
};

static void ElfMembrosConferir(int *total, int *falhas, bool ok, const char *rotulo)
{
    (*total)++;
    if (!ok) {
        (*falhas)++;
        fprintf(stderr, "FALHA teste_elf_membros: %s\n", rotulo);
    }
}

/* Uma secao viva por classe; OS_TDATA, OS_TBSS e OS_INIT_ARRAY vao para 'outros' (nenhuma nestas 4 libs). */
static void ElfMembrosContarSecao(const ObjSecao *s, ElfMembrosContagem *c)
{
    switch (s->tipo) {
    case OS_TEXTO: c->texto++; break;
    case OS_RDATA: c->rdata++; break;
    case OS_DATA: c->data++; break;
    case OS_BSS: c->bss++; break;
    default: c->outros++; break;
    }
    if (LigarSaidaElf(s) < 0)
        c->sem_saida++;
}

static void ElfMembrosContarObjeto(const Objeto *o, ElfMembrosContagem *c)
{
    for (uint32_t i = 0; i < o->secoes.n; i++)
        ElfMembrosContarSecao(VecEm(&o->secoes, i), c);
}

static void ElfMembrosUmMembro(const ArMembro *m, ElfMembrosContagem *c)
{
    Objeto o;
    ElfLErro erro = {0};
    if (!ArEhElf(m->dados, m->tam)) {
        c->erros++;
        return;
    }
    ObjIniciar(&o);
    if (ElfLerObjeto(m->dados, m->tam, &o, &erro) == ELFL_OK) {
        c->membros++;
        ElfMembrosContarObjeto(&o, c);
    } else {
        c->erros++;
        fprintf(stderr, "teste_elf_membros: membro ELF recusado: %s\n", erro.mensagem);
    }
    ObjLiberar(&o);
}

/* Percorre os membros de uma lib; membro que nao e objeto (indice e tabela de nomes) fica de fora. */
static void ElfMembrosUmaLib(const char *caminho, ElfMembrosContagem *c)
{
    char *dados = NULL;
    size_t tam = 0;
    ArIterador it;
    ArMembro m;
    Diag erro = {0};
    if (!ArquivoLerInteiro(caminho, &dados, &tam)) {
        c->erros++;
        fprintf(stderr, "teste_elf_membros: nao leu %s\n", caminho);
        return;
    }
    if (!ArIniciar(&it, (const uint8_t *)dados, tam, &erro))
        c->erros++;
    while (erro.tem == false && ArProximo(&it, &m, &erro)) {
        if (m.tipo == AR_OBJETO)
            ElfMembrosUmMembro(&m, c);
    }
    if (erro.tem)
        c->erros++;
    free(dados);
}

/* Teste do ligador: a saida de OS_RDATA e a de .rdata; OS_TDATA e OS_TBSS vao para o bloco TLS (M5c-b P61); TLS sem musl = X0113. */
static void ElfMembrosSaidasSinteticas(int *total, int *falhas)
{
    ObjSecao rdata = {.tipo = OS_RDATA};
    ObjSecao tdata = {.tipo = OS_TDATA};
    ObjSecao tbss = {.tipo = OS_TBSS};
    LigarPedido pedido = {0};
    Objeto semente_falsa = {0};
    LigacaoEstado L = {0};
    Diag erro = {0};
    L.erro = &erro;
    L.pedido = &pedido;
    ElfMembrosConferir(total, falhas, LigarSaidaElf(&rdata) == LIGAR_SAIDA_RDATA, "OS_RDATA sem a saida .rdata do ELF");
    ElfMembrosConferir(total, falhas, LigarSaidaElf(&tdata) == LIGAR_SAIDA_TLS && LigarSaidaElf(&tbss) == LIGAR_SAIDA_TLS, "OS_TDATA/OS_TBSS sem a saida TLS");
    ElfMembrosConferir(total, falhas, !LigarElfTlsPermitido(&L, &tdata) && erro.tem && erro.codigo == X0113, "TLS sem musl sem X0113");
    erro = (Diag){0};
    pedido.semente = &semente_falsa;
    ElfMembrosConferir(total, falhas, LigarElfTlsPermitido(&L, &tdata) && !erro.tem, "TLS com a semente musl recusado");
    erro = (Diag){0};
    ElfMembrosConferir(total, falhas, !LigarElfSemSaida(&L, &rdata) && erro.tem && erro.codigo == X0903, "tipo sem saida sem X0903");
}

/* Teste do relaxamento GOTTPOFF (M5c-b P61): so mov e add [rip+d] sem REX.R relaxam; o resto e X0114. */
static void ElfMembrosFormaTls(int *total, int *falhas)
{
    static const uint8_t mov[] = {0x48, 0x8b, 0x05, 0, 0, 0, 0};
    static const uint8_t add[] = {0x48, 0x03, 0x05, 0, 0, 0, 0};
    static const uint8_t movr8[] = {0x4c, 0x8b, 0x05, 0, 0, 0, 0};
    static const uint8_t direto[] = {0x48, 0x8b, 0xc0, 0, 0, 0, 0};
    ObjSecao s = {.tipo = OS_TEXTO};
    s.bytes.dados = (char *)mov;
    s.bytes.tam = sizeof mov;
    ElfMembrosConferir(total, falhas, LigarElfFormaTls(&s, 3) == 0x8b, "GOTTPOFF mov [rip+d] nao reconhecido");
    s.bytes.dados = (char *)add;
    ElfMembrosConferir(total, falhas, LigarElfFormaTls(&s, 3) == 0x03, "GOTTPOFF add [rip+d] nao reconhecido");
    s.bytes.dados = (char *)movr8;
    ElfMembrosConferir(total, falhas, LigarElfFormaTls(&s, 3) == 0, "GOTTPOFF com REX.R aceito (deveria ser X0114)");
    s.bytes.dados = (char *)direto;
    ElfMembrosConferir(total, falhas, LigarElfFormaTls(&s, 3) == 0, "GOTTPOFF com modrm direto aceito (deveria ser X0114)");
}

void ElfMembrosVerificacoes(int *total, int *falhas)
{
    ElfMembrosContagem c = {0};
    for (int i = 0; i < ELF_MEMBROS_QTD_LIBS; i++)
        ElfMembrosUmaLib(ELF_MEMBROS_LIBS[i], &c);
    ElfMembrosConferir(total, falhas, c.erros == 0, "membro das 4 libs do Linux recusado ou ilegivel");
    ElfMembrosConferir(total, falhas, c.membros == ELF_MEMBROS_ESPERADO_MEMBROS, "quantidade de membros ELF != censo (19)");
    ElfMembrosConferir(total, falhas, c.texto == ELF_MEMBROS_ESPERADO_TEXTO, "secoes OS_TEXTO != censo (28)");
    ElfMembrosConferir(total, falhas, c.rdata == ELF_MEMBROS_ESPERADO_RDATA, "secoes OS_RDATA != censo (45)");
    ElfMembrosConferir(total, falhas, c.data == ELF_MEMBROS_ESPERADO_DATA, "secoes OS_DATA != censo (19)");
    ElfMembrosConferir(total, falhas, c.bss == ELF_MEMBROS_ESPERADO_BSS, "secoes OS_BSS != censo (19)");
    ElfMembrosConferir(total, falhas, c.outros == 0, "secao TLS ou init_array inesperada nas 4 libs");
    ElfMembrosConferir(total, falhas, c.sem_saida == 0, "secao viva sem saida no ligador ELF");
    ElfMembrosSaidasSinteticas(total, falhas);
    ElfMembrosFormaTls(total, falhas);
}
