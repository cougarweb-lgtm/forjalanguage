/* teste_estatica.c - verificacoes de unidade da selecao de membros de bibliotecas estaticas (EstaticaVerificacoes, chamada pelo teste_nativo): 6 verificacoes do P23d, e o modo
 * de linha de comando "estatica" (EstaticaModo) que o oraculo do mapa do ld usa.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P35: membros ELF no mesmo .a dos COFF (MembroTeste.maquina >= MAQUINA_ELF monta ELF64 relocavel x86-64 com .text, symtab, strtab e RELA opcional):
 *                    ELF puxado pelo indice e indefinido resolvido por outro ELF; COFF e ELF misturados; ElfLerObjeto preenche o Objeto (WEAK, relocacao PC32 = OR_REL32); tipo
 *                    R_X86_64_TLSGD (19) fora do censo = ELFL_RELOC_FORA_DO_CENSO e ArRelatarMembroElf = X0110 com o tipo e o membro.
 *            0.1.0 - M3b P23d: indice GNU (big-endian); 2o indice MS (little-endian) que vence o 1o; ar sem indice, "/SYM64/" e indice truncado = X0105; ponto fixo entre duas estaticas
 *                    (volta para a primeira, pendentes, ordem); X0106 e arquivo repetido; offset de indice que nao e membro e membro de 32 bits = X0105.
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/elf_ler.h"
#include "nativo/obj/estatica.h"
#include "nativo/obj/objeto.h"

#define MAQUINA_ELF 0xE1Fu        /* membro ELF sem relocacao */
#define MAQUINA_ELF_PC32 0xE20u   /* membro ELF com R_X86_64_PC32 (2) em .text, contra o 1o indefinido */
#define MAQUINA_ELF_TLSGD 0xE21u  /* membro ELF com R_X86_64_TLSGD (19), fora do censo */
#define MAQUINA_AMD64 0x8664u
#define MAQUINA_I386 0x14Cu
#define TAM_CABECALHO_AR 60u
#define TAM_COFF 20u
#define TAM_SECAO 40u
#define TAM_SIMBOLO 18u
#define TAM_ASSINATURA_AR 8u
#define CLASSE_EXTERNA 2u
#define MAX_NOMES 8u

typedef struct {
    int *total;
    int *falhas;
} Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (estatica) linha %d: %s\n", linha, texto);
}

#define ESTATICA_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* ---------- montagem de COFF e de ar, na mao ---------- */

static void Anexar16(Buf *b, uint32_t v)
{
    const char dois[2] = {(char)(v & 0xFFu), (char)((v >> 8) & 0xFFu)};
    BufAnexarN(b, dois, sizeof dois);
}

static void Anexar32(Buf *b, uint32_t v)
{
    Anexar16(b, v & 0xFFFFu);
    Anexar16(b, v >> 16);
}

static void Anexar32Be(Buf *b, uint32_t v)
{
    const char quatro[4] = {(char)(v >> 24), (char)((v >> 16) & 0xFFu), (char)((v >> 8) & 0xFFu), (char)(v & 0xFFu)};
    BufAnexarN(b, quatro, sizeof quatro);
}

/* Um membro de teste: o que define e o que referencia (listas terminadas em NULL). maquina 0 = AMD64. */
typedef struct {
    const char *nome;
    const char *const *def;
    const char *const *indef;
    uint16_t maquina;  /* MAQUINA_ELF* = membro ELF (o tipo de relocacao sai de TipoRelocDoMembro); senao a maquina do COFF (0 = AMD64) */
} MembroTeste;

static size_t Contar(const char *const *lista)
{
    size_t n = 0;
    while (lista != NULL && lista[n] != NULL)
        n++;
    return n;
}

/* Um simbolo COFF de 18 bytes: nome em linha (ate 8) ou na tabela de strings; indefinido = secao 0, definido = secao 1. */
static void AnexarSimbolo(Buf *simbolos, Buf *strtab, const char *nome, bool definido)
{
    char campo[8] = {0};
    size_t tam = strlen(nome);
    if (tam <= sizeof campo) {
        memcpy(campo, nome, tam);
        BufAnexarN(simbolos, campo, sizeof campo);
    } else {
        Anexar32(simbolos, 0);
        Anexar32(simbolos, (uint32_t)(4 + strtab->tam));
        BufAnexarN(strtab, nome, tam + 1);
    }
    Anexar32(simbolos, 0);
    Anexar16(simbolos, definido ? 1u : 0u);
    Anexar16(simbolos, 0);
    BufAnexarN(simbolos, "\x02", 1);  /* classe EXTERNAL */
    BufAnexarN(simbolos, "\x00", 1);
}

static void Anexar64(Buf *b, uint64_t v)
{
    Anexar32(b, (uint32_t)(v & 0xFFFFFFFFu));
    Anexar32(b, (uint32_t)(v >> 32));
}

static void AnexarZeros(Buf *b, size_t n)
{
    char zeros[64] = {0};
    while (n > 0) {
        size_t k = n < sizeof zeros ? n : sizeof zeros;
        BufAnexarN(b, zeros, k);
        n -= k;
    }
}

/* Campos de uma secao ELF64 de teste (o cabecalho tem 64 bytes: nome no shstrtab, tipo, flags, offset, tamanho, link, info, alinhamento 8, entsize). */
typedef struct {
    uint32_t nome, tipo, link, info;
    uint64_t flags, off, tam, entsize;
} ShdrTeste;

static void AnexarShdr(Buf *b, const ShdrTeste *s)
{
    Anexar32(b, s->nome);
    Anexar32(b, s->tipo);
    Anexar64(b, s->flags);
    Anexar64(b, 0);
    Anexar64(b, s->off);
    Anexar64(b, s->tam);
    Anexar32(b, s->link);
    Anexar32(b, s->info);
    Anexar64(b, 8);
    Anexar64(b, s->entsize);
}

/* Tipo de relocacao do membro ELF de teste (0 = nenhuma): so os dois sentinelas com relocacao. */
static uint32_t TipoRelocDoMembro(uint16_t maquina)
{
    if (maquina == MAQUINA_ELF_PC32)
        return 2u;
    if (maquina == MAQUINA_ELF_TLSGD)
        return 19u;
    return 0u;
}

/* Simbolo ELF64 de 24 bytes: GLOBAL FUNC definido na .text (secao 1, valor = o indice * 4) ou GLOBAL NOTYPE indefinido (secao 0).
 * LIMITE: parametros 5 (sao os campos do simbolo; um struct so repetiria o nome). */
static void AnexarSimboloElf(Buf *sym, Buf *strtab, const char *nome, bool definido, uint64_t valor)
{
    Anexar32(sym, (uint32_t)strtab->tam);
    BufAnexarN(sym, definido ? "\x12" : "\x10", 1);
    BufAnexarN(sym, "\x00", 1);
    Anexar16(sym, definido ? 1u : 0u);
    Anexar64(sym, valor);
    Anexar64(sym, definido ? 4u : 0u);
    BufAnexarN(strtab, nome, strlen(nome) + 1);
}

#define ELF_TAM_EHDR_TESTE 64u
#define ELF_TAM_SIMBOLO_TESTE 24u
#define ELF_TAM_SHSTRTAB_TESTE 44u
#define ELF_TAM_RELA_TESTE 24u
#define ELF_TAM_SHDR_TESTE 64u
#define ELF_TEXTO_TESTE 16u

/* Membro ELF64 relocavel x86-64 minimo: .text (16 bytes), .symtab, .strtab, .shstrtab e .rela.text quando ha relocacao. O indice da 1a
 * symtab global e 1 (o 1o definido) e os definidos vem antes dos indefinidos, como o GNU as grava. */
static void MontarElf(Buf *out, const MembroTeste *m)
{
    Buf sym = {0}, strtab = {0};
    size_t nd = Contar(m->def), ni = Contar(m->indef);
    uint32_t tipo_reloc = TipoRelocDoMembro(m->maquina);
    bool com_rela = tipo_reloc != 0;
    uint64_t off_sym, off_str, off_shstr, off_rela, off_shdr;
    AnexarZeros(&sym, ELF_TAM_SIMBOLO_TESTE);
    BufAnexarN(&strtab, "", 1);
    for (size_t i = 0; i < nd; i++)
        AnexarSimboloElf(&sym, &strtab, m->def[i], true, 4u * i);
    for (size_t i = 0; i < ni; i++)
        AnexarSimboloElf(&sym, &strtab, m->indef[i], false, 0);
    off_sym = ELF_TAM_EHDR_TESTE + ELF_TEXTO_TESTE;
    off_str = off_sym + sym.tam;
    off_shstr = off_str + strtab.tam;
    off_rela = (off_shstr + ELF_TAM_SHSTRTAB_TESTE + 7u) & ~7ull;
    off_shdr = (off_rela + (com_rela ? ELF_TAM_RELA_TESTE : 0u) + 7u) & ~7ull;
    BufAnexarN(out, "\x7f" "ELF", 4);
    BufAnexarN(out, "\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", 12);
    Anexar16(out, 1);
    Anexar16(out, 62);
    Anexar32(out, 1);
    Anexar64(out, 0);
    Anexar64(out, 0);
    Anexar64(out, off_shdr);
    Anexar32(out, 0);
    Anexar16(out, ELF_TAM_EHDR_TESTE);
    Anexar16(out, 0);
    Anexar16(out, 0);
    Anexar16(out, ELF_TAM_SHDR_TESTE);
    Anexar16(out, com_rela ? 6u : 5u);
    Anexar16(out, 4);
    AnexarZeros(out, ELF_TEXTO_TESTE);
    BufAnexarN(out, sym.dados, sym.tam);
    BufAnexarN(out, strtab.dados, strtab.tam);
    BufAnexarN(out, "\x00.text\x00.symtab\x00.strtab\x00.shstrtab\x00.rela.text\x00", ELF_TAM_SHSTRTAB_TESTE);
    AnexarZeros(out, (size_t)(off_rela - off_shstr - ELF_TAM_SHSTRTAB_TESTE));
    if (com_rela) {
        uint32_t alvo = (uint32_t)(ni > 0 ? 1 + nd : 1);
        Anexar64(out, 0);
        Anexar64(out, ((uint64_t)alvo << 32) | tipo_reloc);
        Anexar64(out, 0);
    }
    AnexarZeros(out, (size_t)(off_shdr - (off_rela + (com_rela ? ELF_TAM_RELA_TESTE : 0u))));
    AnexarShdr(out, &(ShdrTeste){0});
    AnexarShdr(out, &(ShdrTeste){.nome = 1, .tipo = 1, .flags = 6, .off = ELF_TAM_EHDR_TESTE, .tam = ELF_TEXTO_TESTE});
    AnexarShdr(out, &(ShdrTeste){.nome = 7, .tipo = 2, .off = off_sym, .tam = sym.tam, .link = 3, .info = 1, .entsize = ELF_TAM_SIMBOLO_TESTE});
    AnexarShdr(out, &(ShdrTeste){.nome = 15, .tipo = 3, .off = off_str, .tam = strtab.tam});
    AnexarShdr(out, &(ShdrTeste){.nome = 23, .tipo = 3, .off = off_shstr, .tam = ELF_TAM_SHSTRTAB_TESTE});
    if (com_rela)
        AnexarShdr(out, &(ShdrTeste){.nome = 33, .tipo = 4, .off = off_rela, .tam = ELF_TAM_RELA_TESTE, .link = 2, .info = 1, .entsize = ELF_TAM_RELA_TESTE});
    BufLiberar(&sym);
    BufLiberar(&strtab);
}

/* Objeto COFF so com cabecalho, 1 secao vazia, tabela de simbolos e tabela de strings (o selecionador nao le secoes nem relocacoes). */
static void MontarCoff(Buf *out, const MembroTeste *m)
{
    Buf simbolos = {0}, strtab = {0};
    size_t nd = Contar(m->def), ni = Contar(m->indef);
    char zeros[TAM_SECAO] = {0};
    for (size_t i = 0; i < nd; i++)
        AnexarSimbolo(&simbolos, &strtab, m->def[i], true);
    for (size_t i = 0; i < ni; i++)
        AnexarSimbolo(&simbolos, &strtab, m->indef[i], false);
    Anexar16(out, m->maquina != 0 ? m->maquina : MAQUINA_AMD64);
    Anexar16(out, 1);
    Anexar32(out, 0);
    Anexar32(out, TAM_COFF + TAM_SECAO);
    Anexar32(out, (uint32_t)(nd + ni));
    Anexar16(out, 0);
    Anexar16(out, 0);
    BufAnexarN(out, zeros, sizeof zeros);
    if (simbolos.tam > 0)
        BufAnexarN(out, simbolos.dados, simbolos.tam);
    Anexar32(out, (uint32_t)(4 + strtab.tam));
    if (strtab.tam > 0)
        BufAnexarN(out, strtab.dados, strtab.tam);
    BufLiberar(&simbolos);
    BufLiberar(&strtab);
}

static void AnexarMembroAr(Buf *ar, const char *nome, const void *dados, size_t tam)
{
    char cab[TAM_CABECALHO_AR + 1];
    snprintf(cab, sizeof cab, "%-16s%-12s%-6s%-6s%-8s%-10zu`\n", nome, "0", "0", "0", "100644", tam);
    BufAnexarN(ar, cab, TAM_CABECALHO_AR);
    if (tam > 0)
        BufAnexarN(ar, dados, tam);
    if ((tam & 1u) != 0)
        BufAnexarN(ar, "\n", 1);
}

static size_t Par(size_t n)
{
    return n + (n & 1u);
}

/* Formatos de indice de simbolos que MontarAr grava. */
typedef enum { IDX_GNU, IDX_MS, IDX_NENHUM, IDX_SYM64 } IndiceFormato;

typedef struct {
    Buf objetos[MAX_NOMES];
    size_t offsets[MAX_NOMES];
    uint32_t qtd;
    size_t qtd_simbolos;
    size_t tam_nomes;
} Plano;

/* Calcula os objetos e (dado o tamanho dos membros de indice) o offset do cabecalho de cada membro. */
static void PlanejarAr(Plano *p, const MembroTeste *ms, uint32_t qtd, size_t tam_indices)
{
    size_t pos = TAM_ASSINATURA_AR + tam_indices;
    p->qtd = qtd;
    for (uint32_t i = 0; i < qtd; i++) {
        if (ms[i].maquina >= MAQUINA_ELF)
            MontarElf(&p->objetos[i], &ms[i]);
        else
            MontarCoff(&p->objetos[i], &ms[i]);
        p->offsets[i] = pos;
        pos += TAM_CABECALHO_AR + Par(p->objetos[i].tam);
    }
}

static void ContarSimbolos(Plano *p, const MembroTeste *ms, uint32_t qtd)
{
    p->qtd = qtd;
    p->qtd_simbolos = 0;
    p->tam_nomes = 0;
    for (uint32_t i = 0; i < qtd; i++) {
        for (size_t k = 0; k < Contar(ms[i].def); k++) {
            p->qtd_simbolos++;
            p->tam_nomes += strlen(ms[i].def[k]) + 1;
        }
    }
}

/* 1o linker member: N, N offsets (um por simbolo) e os nomes, big-endian. */
static void AnexarIndiceGnu(Buf *ar, const Plano *p, const MembroTeste *ms)
{
    Buf c = {0};
    Anexar32Be(&c, (uint32_t)p->qtd_simbolos);
    for (uint32_t i = 0; i < p->qtd; i++) {
        for (size_t k = 0; k < Contar(ms[i].def); k++)
            Anexar32Be(&c, (uint32_t)p->offsets[i]);
    }
    for (uint32_t i = 0; i < p->qtd; i++) {
        for (size_t k = 0; k < Contar(ms[i].def); k++)
            BufAnexarN(&c, ms[i].def[k], strlen(ms[i].def[k]) + 1);
    }
    AnexarMembroAr(ar, "/", c.dados, c.tam);
    BufLiberar(&c);
}

/* 2o linker member MS: M offsets, N, N numeros de membro (16 bits, base 1) e os nomes, little-endian. */
static void AnexarIndiceMs2(Buf *ar, const Plano *p, const MembroTeste *ms)
{
    Buf c = {0};
    Anexar32(&c, p->qtd);
    for (uint32_t i = 0; i < p->qtd; i++)
        Anexar32(&c, (uint32_t)p->offsets[i]);
    Anexar32(&c, (uint32_t)p->qtd_simbolos);
    for (uint32_t i = 0; i < p->qtd; i++) {
        for (size_t k = 0; k < Contar(ms[i].def); k++)
            Anexar16(&c, i + 1);
    }
    for (uint32_t i = 0; i < p->qtd; i++) {
        for (size_t k = 0; k < Contar(ms[i].def); k++)
            BufAnexarN(&c, ms[i].def[k], strlen(ms[i].def[k]) + 1);
    }
    AnexarMembroAr(ar, "/", c.dados, c.tam);
    BufLiberar(&c);
}

static size_t TamIndiceGnu(const Plano *p)
{
    return TAM_CABECALHO_AR + Par(4 + 4 * p->qtd_simbolos + p->tam_nomes);
}

static size_t TamIndiceMs2(const Plano *p)
{
    return TAM_CABECALHO_AR + Par(4 + 4 * (size_t)p->qtd + 4 + 2 * p->qtd_simbolos + p->tam_nomes);
}

/* O tamanho dos membros de indice que cada formato grava (o MS grava o 1o vazio: quem vale e o 2o). */
static size_t TamIndices(const Plano *p, IndiceFormato f)
{
    if (f == IDX_GNU)
        return TamIndiceGnu(p);
    if (f == IDX_MS)
        return TAM_CABECALHO_AR + TamIndiceMs2(p);
    if (f == IDX_SYM64)
        return TAM_CABECALHO_AR;
    return 0;
}

static void AnexarIndices(Buf *ar, const Plano *p, const MembroTeste *ms, IndiceFormato f)
{
    if (f == IDX_GNU)
        AnexarIndiceGnu(ar, p, ms);
    if (f == IDX_MS) {
        AnexarMembroAr(ar, "/", NULL, 0);
        AnexarIndiceMs2(ar, p, ms);
    }
    if (f == IDX_SYM64)
        AnexarMembroAr(ar, "/SYM64/", NULL, 0);
}

static void MontarAr(Buf *ar, const MembroTeste *ms, uint32_t qtd, IndiceFormato f)
{
    Plano p = {0};
    ContarSimbolos(&p, ms, qtd);
    PlanejarAr(&p, ms, qtd, TamIndices(&p, f));
    BufAnexar(ar, "!<arch>\n");
    AnexarIndices(ar, &p, ms, f);
    for (uint32_t i = 0; i < qtd; i++) {
        AnexarMembroAr(ar, ms[i].nome, p.objetos[i].dados, p.objetos[i].tam);
        BufLiberar(&p.objetos[i]);
    }
}

/* ---------- consultas ---------- */

static bool Selecionar(const EstaticaArquivo *arqs, uint32_t qtd, const char *const *raizes, uint32_t nraizes, EstaticaSelecao *sel, Diag *erro)
{
    EstaticaPedido pedido = {.arquivos = arqs, .qtd_arquivos = qtd, .raizes = raizes, .qtd_raizes = nraizes};
    return EstaticaSelecionar(&pedido, sel, erro);
}

static bool MembroEh(const EstaticaSelecao *sel, size_t i, uint32_t arquivo, const char *nome)
{
    const EstaticaMembro *m = VecEm(&sel->membros, i);
    return i < sel->membros.n && m->arquivo == arquivo && strcmp(m->nome, nome) == 0;
}

static bool PendenteEh(const EstaticaSelecao *sel, size_t i, const char *nome)
{
    return i < sel->pendentes.n && strcmp(*(const char *const *)VecEm(&sel->pendentes, i), nome) == 0;
}

static bool ArRecusaComX0105(const Buf *ar, const char *frase)
{
    Vec simbolos;
    Diag erro = {0};
    bool lido = ArLerIndice((const uint8_t *)ar->dados, ar->tam, &simbolos, &erro);
    VecLiberar(&simbolos);
    return !lido && erro.tem && erro.codigo == X0105 && strstr(erro.causa, frase) != NULL;
}

/* ---------- 1: 1o linker member GNU (big-endian) ---------- */

static const char *const DEF_A[] = {"fa", "fa_longa_demais", NULL};
static const char *const DEF_C[] = {"fc", NULL};

static bool IndiceGnuBigEndian(void)
{
    const MembroTeste ms[] = {{"a.o/", DEF_A, NULL, 0}, {"c.o/", DEF_C, NULL, 0}};
    Buf ar = {0};
    Vec simbolos;
    Diag erro = {0};
    ArIterador it;
    ArMembro m, a, c;
    bool ok;
    MontarAr(&ar, ms, 2, IDX_GNU);
    ok = ArLerIndice((const uint8_t *)ar.dados, ar.tam, &simbolos, &erro) && simbolos.n == 3;
    ok = ok && ArIniciar(&it, (const uint8_t *)ar.dados, ar.tam, &erro) && ArProximo(&it, &m, &erro) && m.tipo == AR_INDICE;
    ok = ok && ArProximo(&it, &a, &erro) && ArProximo(&it, &c, &erro);
    ok = ok && ((const ArSimbolo *)VecEm(&simbolos, 0))->membro_off == a.off && strcmp(((const ArSimbolo *)VecEm(&simbolos, 0))->nome, "fa") == 0;
    ok = ok && ((const ArSimbolo *)VecEm(&simbolos, 1))->membro_off == a.off && ((const ArSimbolo *)VecEm(&simbolos, 1))->nome_tam == strlen("fa_longa_demais");
    ok = ok && ((const ArSimbolo *)VecEm(&simbolos, 2))->membro_off == c.off && strcmp(((const ArSimbolo *)VecEm(&simbolos, 2))->nome, "fc") == 0;
    VecLiberar(&simbolos);
    BufLiberar(&ar);
    return ok;
}

/* ---------- 2: 2o linker member MS (little-endian) vence o 1o ---------- */

static bool IndiceMsSegundoVence(void)
{
    const MembroTeste ms[] = {{"a.o/", DEF_A, NULL, 0}, {"c.o/", DEF_C, NULL, 0}};
    Buf ar = {0};
    Vec simbolos;
    Diag erro = {0};
    ArIterador it;
    ArMembro m, a, c;
    bool ok;
    MontarAr(&ar, ms, 2, IDX_MS);  /* o 1o "/" vem vazio: so o 2o diz quem define o que */
    ok = ArLerIndice((const uint8_t *)ar.dados, ar.tam, &simbolos, &erro) && simbolos.n == 3;
    ok = ok && ArIniciar(&it, (const uint8_t *)ar.dados, ar.tam, &erro) && ArProximo(&it, &m, &erro) && ArProximo(&it, &m, &erro);
    ok = ok && ArProximo(&it, &a, &erro) && ArProximo(&it, &c, &erro);
    ok = ok && ((const ArSimbolo *)VecEm(&simbolos, 0))->membro_off == a.off && ((const ArSimbolo *)VecEm(&simbolos, 1))->membro_off == a.off;
    ok = ok && ((const ArSimbolo *)VecEm(&simbolos, 2))->membro_off == c.off && strcmp(((const ArSimbolo *)VecEm(&simbolos, 2))->nome, "fc") == 0;
    VecLiberar(&simbolos);
    BufLiberar(&ar);
    return ok;
}

/* ---------- 3: sem indice, /SYM64/ e indice truncado = X0105 ---------- */

static bool SemIndiceSym64Truncado(void)
{
    const MembroTeste ms[] = {{"a.o/", DEF_A, NULL, 0}};
    Buf sem = {0}, sym64 = {0}, truncado = {0};
    bool ok;
    MontarAr(&sem, ms, 1, IDX_NENHUM);
    MontarAr(&sym64, ms, 1, IDX_SYM64);
    MontarAr(&truncado, ms, 1, IDX_GNU);
    truncado.tam = TAM_ASSINATURA_AR + TAM_CABECALHO_AR + 6;  /* o corpo do indice acaba no meio da tabela de offsets */
    memcpy(truncado.dados + TAM_ASSINATURA_AR + 48, "6         ", 10);  /* o cabecalho do indice passa a dizer 6 bytes */
    ok = ArRecusaComX0105(&sem, "indice de simbolos") && ArRecusaComX0105(&sym64, "SYM64") && ArRecusaComX0105(&truncado, "truncado");
    BufLiberar(&sem);
    BufLiberar(&sym64);
    BufLiberar(&truncado);
    return ok;
}

/* ---------- 4: ponto fixo entre duas estaticas ---------- */

static const char *const DEF_FA[] = {"fa", NULL}, *const IND_FA[] = {"fb_nome_longo", NULL};
static const char *const DEF_FB[] = {"fb_nome_longo", NULL}, *const IND_FB[] = {"fc", "__imp_ext", NULL};
static const char *const DEF_FZ[] = {"fz", NULL};

/* libA: fa -> fb (libB) -> fc (libA, atras de quem a precisou: o ld, que varre cada .a uma vez, nao acharia); fz ninguem pede. */
static bool PontoFixoEntreDuasEstaticas(void)
{
    const MembroTeste lib_a[] = {{"fa.o/", DEF_FA, IND_FA, 0}, {"fc.o/", DEF_C, NULL, 0}, {"fz.o/", DEF_FZ, NULL, 0}};
    const MembroTeste lib_b[] = {{"fb.o/", DEF_FB, IND_FB, 0}};
    const char *const raizes[] = {"fa", "nao_existe"};
    Buf a = {0}, b = {0};
    EstaticaSelecao sel;
    Diag erro = {0};
    bool ok;
    MontarAr(&a, lib_a, 3, IDX_GNU);
    MontarAr(&b, lib_b, 1, IDX_MS);
    const EstaticaArquivo arqs[] = {{"libA.a", (const uint8_t *)a.dados, a.tam, NULL}, {"libB.lib", (const uint8_t *)b.dados, b.tam, NULL}};
    ok = Selecionar(arqs, 2, raizes, 2, &sel, &erro) && sel.membros.n == 3;
    ok = ok && MembroEh(&sel, 0, 0, "fa.o") && MembroEh(&sel, 1, 1, "fb.o") && MembroEh(&sel, 2, 0, "fc.o");
    ok = ok && sel.pendentes.n == 2 && PendenteEh(&sel, 0, "nao_existe") && PendenteEh(&sel, 1, "__imp_ext");
    EstaticaLiberar(&sel);
    BufLiberar(&a);
    BufLiberar(&b);
    return ok;
}

/* ---------- 5: X0106, arquivo repetido e duas definicoes no mesmo arquivo ---------- */

static const char *const DEF_DUP[] = {"dup", NULL};

static bool DuasEstaticasDefinindoOMesmoSimbolo(void)
{
    const MembroTeste um[] = {{"dup1.o/", DEF_DUP, NULL, 0}};
    const MembroTeste dois[] = {{"dup1.o/", DEF_DUP, NULL, 0}, {"dup2.o/", DEF_DUP, NULL, 0}};
    const char *const raizes[] = {"dup"};
    Buf a = {0}, b = {0}, c = {0};
    EstaticaSelecao sel, sel2;
    Diag erro = {0}, erro2 = {0};
    bool ok;
    MontarAr(&a, um, 1, IDX_GNU);
    MontarAr(&b, um, 1, IDX_GNU);
    MontarAr(&c, dois, 2, IDX_GNU);
    const EstaticaArquivo duas[] = {{"libA.a", (const uint8_t *)a.dados, a.tam, NULL}, {"libB.a", (const uint8_t *)b.dados, b.tam, NULL}};
    const EstaticaArquivo repetida[] = {duas[0], duas[0], {"libC.a", (const uint8_t *)c.dados, c.tam, NULL}};
    ok = !Selecionar(duas, 2, raizes, 1, &sel, &erro) && erro.tem && erro.codigo == X0106;
    ok = ok && strstr(erro.causa, "'dup'") != NULL && strstr(erro.causa, "libA.a") != NULL && strstr(erro.causa, "libB.a") != NULL;
    EstaticaLiberar(&sel);
    ok = ok && Selecionar(repetida, 2, raizes, 1, &sel2, &erro2) && sel2.membros.n == 1 && MembroEh(&sel2, 0, 0, "dup1.o");
    EstaticaLiberar(&sel2);
    ok = ok && Selecionar(&repetida[2], 1, raizes, 1, &sel2, &erro2) && sel2.membros.n == 1 && MembroEh(&sel2, 0, 0, "dup1.o");
    EstaticaLiberar(&sel2);
    BufLiberar(&a);
    BufLiberar(&b);
    BufLiberar(&c);
    return ok;
}

/* ---------- 6: indice que manda para onde nao ha membro; membro de 32 bits ---------- */

static const char *const IND_I386[] = {"x", NULL};

static bool IndiceTortoEMembroDe32Bits(void)
{
    const MembroTeste bom[] = {{"fa.o/", DEF_FA, NULL, 0}};
    const MembroTeste i386[] = {{"fa.o/", DEF_FA, IND_I386, MAQUINA_I386}};
    const char *const raizes[] = {"fa"};
    Buf torto = {0}, de32 = {0};
    EstaticaSelecao sel, sel2;
    Diag erro = {0}, erro2 = {0};
    bool ok;
    MontarAr(&torto, bom, 1, IDX_GNU);
    torto.dados[TAM_ASSINATURA_AR + TAM_CABECALHO_AR + 4 + 3] = 0x55;  /* o offset do 1o simbolo (big-endian) passa a terminar em 0x55 */
    MontarAr(&de32, i386, 1, IDX_GNU);
    const EstaticaArquivo ta[] = {{"torta.a", (const uint8_t *)torto.dados, torto.tam, NULL}};
    const EstaticaArquivo da[] = {{"de32.a", (const uint8_t *)de32.dados, de32.tam, NULL}};
    ok = !Selecionar(ta, 1, raizes, 1, &sel, &erro) && erro.codigo == X0105 && strstr(erro.causa, "torta.a") != NULL && strstr(erro.causa, "nao e um membro") != NULL;
    EstaticaLiberar(&sel);
    ok = ok && !Selecionar(da, 1, raizes, 1, &sel2, &erro2) && erro2.codigo == X0105 && strstr(erro2.causa, "membro 'fa.o'") != NULL && strstr(erro2.causa, "0x14c") != NULL;
    EstaticaLiberar(&sel2);
    BufLiberar(&torto);
    BufLiberar(&de32);
    return ok;
}

/* ---------- 7 (P35): membros ELF no mesmo .a ---------- */

static const char *const DEF_ELF_CHAMA[] = {"elf_chama", NULL};
static const char *const IND_ELF_CHAMA[] = {"elf_outro", NULL};
static const char *const DEF_ELF_OUTRO[] = {"elf_outro", NULL};
static const char *const DEF_ELF_NAO[] = {"elf_nao_usado", NULL};

/* O ELF e puxado pelo indice; o indefinido dele (elf_outro) puxa o 2o ELF; o 3o nao entra. */
static bool ElfPuxadoPeloIndice(void)
{
    const MembroTeste ms[] = {{"e1.o/", DEF_ELF_CHAMA, IND_ELF_CHAMA, MAQUINA_ELF}, {"e2.o/", DEF_ELF_OUTRO, NULL, MAQUINA_ELF}, {"e3.o/", DEF_ELF_NAO, NULL, MAQUINA_ELF}};
    const char *const raizes[] = {"elf_chama"};
    Buf ar = {0};
    EstaticaSelecao sel;
    Diag erro = {0};
    bool ok;
    MontarAr(&ar, ms, 3, IDX_GNU);
    const EstaticaArquivo arq[] = {{"libelf.a", (const uint8_t *)ar.dados, ar.tam, NULL}};
    ok = Selecionar(arq, 1, raizes, 1, &sel, &erro) && sel.membros.n == 2 && MembroEh(&sel, 0, 0, "e1.o") && MembroEh(&sel, 1, 0, "e2.o");
    ok = ok && sel.pendentes.n == 0;
    EstaticaLiberar(&sel);
    BufLiberar(&ar);
    return ok;
}

/* COFF e ELF no mesmo .a: o ELF pede um simbolo que so o COFF define, e os dois sao puxados. */
static const char *const DEF_MISTO_ELF[] = {"misto_elf", NULL};
static const char *const IND_MISTO_ELF[] = {"misto_coff", NULL};
static const char *const DEF_MISTO_COFF[] = {"misto_coff", NULL};

static bool CoffEElfNoMesmoAr(void)
{
    const MembroTeste ms[] = {{"m_elf.o/", DEF_MISTO_ELF, IND_MISTO_ELF, MAQUINA_ELF}, {"m_coff.o/", DEF_MISTO_COFF, NULL, 0}};
    const char *const raizes[] = {"misto_elf"};
    Buf ar = {0};
    EstaticaSelecao sel;
    Diag erro = {0};
    bool ok;
    MontarAr(&ar, ms, 2, IDX_GNU);
    const EstaticaArquivo arq[] = {{"libmisto.a", (const uint8_t *)ar.dados, ar.tam, NULL}};
    ok = Selecionar(arq, 1, raizes, 1, &sel, &erro) && sel.membros.n == 2 && MembroEh(&sel, 0, 0, "m_elf.o") && MembroEh(&sel, 1, 0, "m_coff.o");
    ok = ok && sel.pendentes.n == 0;
    EstaticaLiberar(&sel);
    BufLiberar(&ar);
    return ok;
}

/* Membro ELF do censo (PC32) vira Objeto: 1 secao (.text), o definido nela, o indefinido sem secao e a relocacao OR_REL32. */
static const char *const DEF_REP[] = {"rep_def", NULL};
static const char *const IND_REP[] = {"rep_ext", NULL};

static bool ElfDoCensoViraObjeto(void)
{
    const MembroTeste ms[] = {{"rep.o/", DEF_REP, IND_REP, MAQUINA_ELF_PC32}};
    const char *const raizes[] = {"rep_def"};
    Buf ar = {0};
    EstaticaSelecao sel;
    Diag erro = {0};
    Objeto o;
    ElfLErro ee = {0};
    int64_t def, ext;
    bool ok;
    MontarAr(&ar, ms, 1, IDX_GNU);
    const EstaticaArquivo arq[] = {{"librep.a", (const uint8_t *)ar.dados, ar.tam, NULL}};
    ok = Selecionar(arq, 1, raizes, 1, &sel, &erro) && sel.membros.n == 1;
    ObjIniciar(&o);
    ok = ok && ElfLerObjeto(sel.membros.n ? ((const EstaticaMembro *)VecEm(&sel.membros, 0))->dados : NULL, sel.membros.n ? ((const EstaticaMembro *)VecEm(&sel.membros, 0))->tam : 0, &o, &ee) == ELFL_OK;
    ok = ok && o.secoes.n == 1 && o.relocs.n == 1 && ((const ObjReloc *)VecEm(&o.relocs, 0))->correcao.tipo == OR_REL32;
    def = ObjAcharSimbolo(&o, "rep_def");
    ext = ObjAcharSimbolo(&o, "rep_ext");
    ok = ok && def >= 0 && ((const ObjSimbolo *)VecEm(&o.simbolos, (size_t)def))->secao == 0;
    ok = ok && ext >= 0 && ((const ObjSimbolo *)VecEm(&o.simbolos, (size_t)ext))->secao == OBJ_SECAO_INDEFINIDA;
    ObjLiberar(&o);
    EstaticaLiberar(&sel);
    BufLiberar(&ar);
    return ok;
}

/* Relocacao R_X86_64_TLSGD (19), fora do censo: a selecao ainda puxa o membro, a conversao recusa com ELFL_RELOC_FORA_DO_CENSO e o relato e X0110 com o tipo e o membro. */
static const char *const DEF_RFC[] = {"rfc_def", NULL};
static const char *const IND_RFC[] = {"rfc_ext", NULL};

static bool RelocacaoForaDoCensoEhX0110(void)
{
    const MembroTeste ms[] = {{"rfc.o/", DEF_RFC, IND_RFC, MAQUINA_ELF_TLSGD}};
    const char *const raizes[] = {"rfc_def"};
    Buf ar = {0};
    EstaticaSelecao sel;
    Diag erro = {0}, relato = {0};
    Objeto o;
    ElfLErro ee = {0};
    const EstaticaMembro *m;
    bool ok;
    MontarAr(&ar, ms, 1, IDX_GNU);
    const EstaticaArquivo arq[] = {{"librfc.a", (const uint8_t *)ar.dados, ar.tam, NULL}};
    ok = Selecionar(arq, 1, raizes, 1, &sel, &erro) && sel.membros.n == 1;
    m = VecEm(&sel.membros, 0);
    ObjIniciar(&o);
    ok = ok && ElfLerObjeto(m->dados, m->tam, &o, &ee) == ELFL_RELOC_FORA_DO_CENSO && ee.tipo_reloc == 19u;
    ok = ok && !ArRelatarMembroElf(&relato, m->nome, &ee) && relato.codigo == X0110;
    ok = ok && strstr(relato.causa, "rfc.o") != NULL && strstr(relato.causa, "tipo 19") != NULL;
    ObjLiberar(&o);
    EstaticaLiberar(&sel);
    BufLiberar(&ar);
    return ok;
}

void EstaticaVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    ESTATICA_VERIFICA(&c, IndiceGnuBigEndian());
    ESTATICA_VERIFICA(&c, IndiceMsSegundoVence());
    ESTATICA_VERIFICA(&c, SemIndiceSym64Truncado());
    ESTATICA_VERIFICA(&c, PontoFixoEntreDuasEstaticas());
    ESTATICA_VERIFICA(&c, DuasEstaticasDefinindoOMesmoSimbolo());
    ESTATICA_VERIFICA(&c, IndiceTortoEMembroDe32Bits());
    ESTATICA_VERIFICA(&c, ElfPuxadoPeloIndice());
    ESTATICA_VERIFICA(&c, CoffEElfNoMesmoAr());
    ESTATICA_VERIFICA(&c, ElfDoCensoViraObjeto());
    ESTATICA_VERIFICA(&c, RelocacaoForaDoCensoEhX0110());
}

/* ---------- modo de linha de comando: o oraculo do mapa do ld ---------- */

static bool LerArquivo(const char *caminho, Buf *bytes)
{
    char bloco[65536];
    size_t n;
    FILE *f = fopen(caminho, "rb");
    if (f == NULL)
        return false;
    while ((n = fread(bloco, 1, sizeof bloco, f)) > 0)
        BufAnexarN(bytes, bloco, n);
    fclose(f);
    return true;
}

/* argv: estatica <lib>... -- <simbolo>...  Imprime "membro <arquivo>(<membro>)" na ordem em que foram puxados e "pendente <simbolo>"; exit 0, 1 se a selecao falhou (JSON do erro
 * na saida de erro) e 2 se a linha de comando ou um arquivo esta errado. */
int EstaticaModo(int argc, char **argv)
{
    int sep = 2;
    while (sep < argc && strcmp(argv[sep], "--") != 0)
        sep++;
    if (sep == 2 || sep == argc)
        return 2;
    uint32_t qtd = (uint32_t)(sep - 2);
    Buf *bytes = calloc(qtd, sizeof *bytes);
    EstaticaArquivo *arqs = calloc(qtd, sizeof *arqs);
    int rc = 0;
    for (uint32_t i = 0; i < qtd && rc == 0; i++) {
        if (!LerArquivo(argv[2 + i], &bytes[i]))
            rc = 2;
        arqs[i] = (EstaticaArquivo){argv[2 + i], (const uint8_t *)bytes[i].dados, bytes[i].tam, NULL};
    }
    EstaticaSelecao sel = {0};
    Diag erro = {0};
    if (rc == 0 && !Selecionar(arqs, qtd, (const char *const *)&argv[sep + 1], (uint32_t)(argc - sep - 1), &sel, &erro)) {
        fprintf(stderr, "%s: %s\n", DiagCodigoTexto(erro.codigo), erro.causa);
        rc = 1;
    }
    for (size_t i = 0; rc == 0 && i < sel.membros.n; i++) {
        const EstaticaMembro *m = VecEm(&sel.membros, i);
        printf("membro %s(%s)\n", arqs[m->arquivo].nome, m->nome);
    }
    for (size_t i = 0; rc == 0 && i < sel.pendentes.n; i++)
        printf("pendente %s\n", *(const char *const *)VecEm(&sel.pendentes, i));
    EstaticaLiberar(&sel);
    for (uint32_t i = 0; i < qtd; i++)
        BufLiberar(&bytes[i]);
    free(bytes);
    free(arqs);
    return rc;
}
