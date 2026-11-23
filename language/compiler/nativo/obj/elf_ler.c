/* elf_ler.c - leitor minimo de ELF64 relocavel x86-64 a partir de um buffer (P33, pista L; M5b P35: ponte para o Objeto neutro).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P35: ElfLerObjeto (secoes, simbolos com WEAK e relocacoes com tipo neutro de objeto.h 0.4.0) e ElfListarIndefinidos; ElfLLerSecao le sh_flags e
 *                    sh_addralign. Censo fixo do libc.a: R_X86_64_{64, PC32, PLT32} viram OR_ADDR64/OR_REL32; 32, 32S, GOTPCREL, GOTPCRELX/REX_GOTPCRELX, TPOFF32 e GOTTPOFF viram
 *                    OR_ABS32, OR_ABS32S, OR_GOTPCREL, OR_GOTPCRELX, OR_TPOFF32 e OR_GOTTPOFF.
 *            0.1.2 - ElfLLerSecao com 4 parametros (conferir_regras: limite CROSS-R05); o erro de secao fora do buffer aponta o off da secao, nao o do Shdr.
 *            0.1.1 - P33: ELF sem SHT_SYMTAB (5 membros do musl: crtn.o, ceill.lo, expm1l.lo, truncl.lo, __invtrigl.lo) le como 0 simbolos em vez de erro.
 *            0.1.0 - P33: cabecalho e tabela de secoes com conferencia de faixa (SHT_NOBITS fica fora da conta, como no arquivo), symtab com a entrada nula
 *                    contada, secoes RELA/REL e censo de relocacoes. Erro por codigo + mensagem, sem exit.
 * Verificacao: gcc -std=c11 -Wall -Wextra -I compiler compiler/nativo/obj/elf_ler.c tests/unidade/teste_elf_ler.c -o teste_elf_ler.exe && teste_elf_ler.exe
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "nativo/obj/elf_ler.h"

#define ELFL_TAM_EHDR 64u
#define ELFL_TAM_SHDR 64u
#define ELFL_TAM_SIMBOLO 24u
#define ELFL_TAM_RELA 24u
#define ELFL_TAM_REL 16u
#define ELFL_ET_REL 1u
#define ELFL_EM_X86_64 62u
#define ELFL_SHT_SYMTAB 2u
#define ELFL_SHT_STRTAB 3u
#define ELFL_SHT_RELA 4u
#define ELFL_SHT_REL 9u
#define ELFL_SHT_NOBITS 8u

typedef struct {
    uint32_t tipo;
    const char *nome;
} ElfLTipoReloc;

/* Censo do link estatico do musl (D-R2/D-R3): tipos fora desta lista sao erro. */
static const ElfLTipoReloc ELFL_CENSO[] = {
    {1, "R_X86_64_64"},         {2, "R_X86_64_PC32"},      {4, "R_X86_64_PLT32"},
    {10, "R_X86_64_32"},        {11, "R_X86_64_32S"},      {9, "R_X86_64_GOTPCREL"},
    {41, "R_X86_64_GOTPCRELX"}, {42, "R_X86_64_REX_GOTPCRELX"},
    {23, "R_X86_64_TPOFF32"},   {22, "R_X86_64_GOTTPOFF"},
};
#define ELFL_TAM_CENSO (sizeof ELFL_CENSO / sizeof ELFL_CENSO[0])

static uint16_t ElfLU16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t ElfLU32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t ElfLU64(const uint8_t *p)
{
    return (uint64_t)ElfLU32(p) | ((uint64_t)ElfLU32(p + 4) << 32);
}

/* Faixa [off, off+tam) dentro de n bytes. A conta nao soma (off + tam pode estourar com cabecalho torto): subtrai. */
static bool ElfLCabe(uint64_t n, uint64_t off, uint64_t tam)
{
    return off <= n && tam <= n - off;
}

static ElfLCodigo ElfLFalha(ElfLErro *erro, const char *motivo, uint64_t off)
{
    erro->codigo = ELFL_FORMATO;
    erro->tipo_reloc = 0;
    snprintf(erro->mensagem, sizeof erro->mensagem, "ELF invalido no offset %llu: %s", (unsigned long long)off, motivo);
    return ELFL_FORMATO;
}

static ElfLCodigo ElfLSemMemoria(ElfLErro *erro)
{
    erro->codigo = ELFL_MEMORIA;
    erro->tipo_reloc = 0;
    snprintf(erro->mensagem, sizeof erro->mensagem, "sem memoria para as tabelas do ELF");
    return ELFL_MEMORIA;
}

/* calloc de pelo menos 1 elemento: contagem zero nao vira NULL, que aqui significaria falta de memoria. */
static void *ElfLAlocar(size_t quantos, size_t tam)
{
    return calloc(quantos > 0 ? quantos : 1, tam);
}

/* Texto NUL-terminado da tabela de strings (tab_off, tab_tam) no indice idx; NULL se o indice cai fora ou o texto nao termina dentro da tabela. */
static const char *ElfLTexto(const uint8_t *bytes, uint64_t tab_off, uint64_t tab_tam, uint64_t idx)
{
    if (idx >= tab_tam)
        return NULL;
    const uint8_t *ini = bytes + tab_off + idx;
    if (memchr(ini, 0, (size_t)(tab_tam - idx)) == NULL)
        return NULL;
    return (const char *)ini;
}

/* Cabecalho ELF: ELF64 little-endian, ET_REL, x86-64, tabela de secoes inteira dentro do buffer e e_shstrndx valido. */
static ElfLCodigo ElfLConferirEhdr(const uint8_t *bytes, uint64_t n, ElfLErro *erro)
{
    static const uint8_t ASSINATURA[4] = {0x7f, 'E', 'L', 'F'};
    if (!ElfLCabe(n, 0, ELFL_TAM_EHDR))
        return ElfLFalha(erro, "menor que o cabecalho ELF de 64 bytes", 0);
    if (memcmp(bytes, ASSINATURA, sizeof ASSINATURA) != 0)
        return ElfLFalha(erro, "falta a assinatura 7f 'E' 'L' 'F'", 0);
    if (bytes[4] != 2)
        return ElfLFalha(erro, "nao e ELF64 (EI_CLASS)", 4);
    if (bytes[5] != 1)
        return ElfLFalha(erro, "nao e little-endian (EI_DATA)", 5);
    if (ElfLU16(bytes + 16) != ELFL_ET_REL)
        return ElfLFalha(erro, "nao e objeto relocavel (e_type diferente de ET_REL)", 16);
    if (ElfLU16(bytes + 18) != ELFL_EM_X86_64)
        return ElfLFalha(erro, "nao e x86-64 (e_machine)", 18);
    if (ElfLU16(bytes + 58) != ELFL_TAM_SHDR)
        return ElfLFalha(erro, "e_shentsize diferente de 64", 58);
    if (ElfLU16(bytes + 60) == 0)
        return ElfLFalha(erro, "sem secoes (e_shnum = 0)", 60);
    if (ElfLU16(bytes + 62) == 0 || ElfLU16(bytes + 62) >= ElfLU16(bytes + 60))
        return ElfLFalha(erro, "e_shstrndx fora da tabela de secoes", 62);
    if (!ElfLCabe(n, ElfLU64(bytes + 40), (uint64_t)ElfLU16(bytes + 60) * ELFL_TAM_SHDR))
        return ElfLFalha(erro, "tabela de secoes fora do buffer", 40);
    return ELFL_OK;
}

/* Campos de uma entrada de secao. A faixa so e conferida fora de SHT_NOBITS: .bss tem tamanho no cabecalho mas nao ocupa o arquivo. */
static ElfLCodigo ElfLLerSecao(const uint8_t *sh, uint64_t n, ElfLSecao *s, ElfLErro *erro)
{
    s->nome = NULL;
    s->tipo = ElfLU32(sh + 4);
    s->off = ElfLU64(sh + 24);
    s->tam = ElfLU64(sh + 32);
    s->link = ElfLU32(sh + 40);
    s->info = ElfLU32(sh + 44);
    s->entsize = ElfLU64(sh + 56);
    s->flags = ElfLU64(sh + 8);
    s->alinhamento = ElfLU64(sh + 48);
    if (s->tipo != ELFL_SHT_NOBITS && !ElfLCabe(n, s->off, s->tam))
        return ElfLFalha(erro, "secao fora do buffer", s->off);
    return ELFL_OK;
}

static ElfLCodigo ElfLLerSecoes(const uint8_t *bytes, uint64_t n, ElfLObjeto *obj, ElfLErro *erro)
{
    uint64_t shoff = ElfLU64(bytes + 40);
    size_t shnum = ElfLU16(bytes + 60);
    obj->secoes = ElfLAlocar(shnum, sizeof *obj->secoes);
    if (obj->secoes == NULL)
        return ElfLSemMemoria(erro);
    obj->n_secoes = shnum;
    for (size_t i = 0; i < shnum; i++) {
        ElfLCodigo cod = ElfLLerSecao(bytes + shoff + i * ELFL_TAM_SHDR, n, &obj->secoes[i], erro);
        if (cod != ELFL_OK)
            return cod;
    }
    return ELFL_OK;
}

/* Nome de cada secao: sh_name indexa a tabela de nomes apontada por e_shstrndx. */
static ElfLCodigo ElfLNomearSecoes(const uint8_t *bytes, ElfLObjeto *obj, ElfLErro *erro)
{
    uint64_t shoff = ElfLU64(bytes + 40);
    const ElfLSecao *tab = &obj->secoes[ElfLU16(bytes + 62)];
    if (tab->tipo != ELFL_SHT_STRTAB)
        return ElfLFalha(erro, "tabela de nomes de secao (e_shstrndx) nao e STRTAB", 62);
    for (size_t i = 0; i < obj->n_secoes; i++) {
        uint64_t off_sh = shoff + i * ELFL_TAM_SHDR;
        obj->secoes[i].nome = ElfLTexto(bytes, tab->off, tab->tam, ElfLU32(bytes + off_sh));
        if (obj->secoes[i].nome == NULL)
            return ElfLFalha(erro, "nome de secao fora da tabela de nomes", off_sh);
    }
    return ELFL_OK;
}

/* Indice da primeira SHT_SYMTAB; n_secoes se nao houver. */
static size_t ElfLAcharSymtab(const ElfLObjeto *obj)
{
    for (size_t i = 0; i < obj->n_secoes; i++) {
        if (obj->secoes[i].tipo == ELFL_SHT_SYMTAB)
            return i;
    }
    return obj->n_secoes;
}

/* A symtab e lida como tabela de entradas de 24 bytes; a strtab e a secao apontada por sh_link. */
static ElfLCodigo ElfLLerSimbolos(const uint8_t *bytes, ElfLObjeto *obj, ElfLErro *erro)
{
    size_t it = ElfLAcharSymtab(obj);
    if (it == obj->n_secoes) {
        obj->n_simbolos = 0; /* ET_REL sem symtab e valido (crtn.o, ceill.lo do musl): 0 simbolos, como o readelf */
        obj->simbolos = NULL;
        return ELFL_OK;
    }
    const ElfLSecao *tab = &obj->secoes[it];
    if (tab->entsize != ELFL_TAM_SIMBOLO || tab->tam % ELFL_TAM_SIMBOLO != 0)
        return ElfLFalha(erro, "symtab com entsize diferente de 24 ou tamanho nao multiplo", tab->off);
    if (tab->link >= obj->n_secoes || obj->secoes[tab->link].tipo != ELFL_SHT_STRTAB)
        return ElfLFalha(erro, "symtab sem strtab valida (sh_link)", tab->off);
    const ElfLSecao *str = &obj->secoes[tab->link];
    obj->n_simbolos = (size_t)(tab->tam / ELFL_TAM_SIMBOLO);
    obj->simbolos = ElfLAlocar(obj->n_simbolos, sizeof *obj->simbolos);
    if (obj->simbolos == NULL)
        return ElfLSemMemoria(erro);
    for (size_t i = 0; i < obj->n_simbolos; i++) {
        const uint8_t *e = bytes + tab->off + i * ELFL_TAM_SIMBOLO;
        ElfLSimbolo *s = &obj->simbolos[i];
        s->nome = ElfLTexto(bytes, str->off, str->tam, ElfLU32(e));
        if (s->nome == NULL)
            return ElfLFalha(erro, "nome de simbolo fora da strtab", (uint64_t)(e - bytes));
        s->info = e[4];
        s->secao = ElfLU16(e + 6);
        s->valor = ElfLU64(e + 8);
        s->tam = ElfLU64(e + 16);
    }
    return ELFL_OK;
}

static bool ElfLEhReloc(const ElfLSecao *s)
{
    return s->tipo == ELFL_SHT_RELA || s->tipo == ELFL_SHT_REL;
}

/* SHT_RELA tem entrada de 24 bytes e SHT_REL de 16; o tamanho da secao tem que ser multiplo exato. */
static ElfLCodigo ElfLConferirSecaoReloc(const ElfLSecao *s, ElfLErro *erro)
{
    uint64_t esperado = s->tipo == ELFL_SHT_RELA ? ELFL_TAM_RELA : ELFL_TAM_REL;
    if (s->entsize != esperado || s->tam % esperado != 0)
        return ElfLFalha(erro, "secao de relocacao com entsize errado ou tamanho nao multiplo", s->off);
    return ELFL_OK;
}

/* Soma as entradas de todas as secoes RELA/REL (o readelf chama isso de "contains N entries"). */
static ElfLCodigo ElfLContarRelocacoes(const ElfLObjeto *obj, size_t *total, ElfLErro *erro)
{
    *total = 0;
    for (size_t i = 0; i < obj->n_secoes; i++) {
        const ElfLSecao *s = &obj->secoes[i];
        if (!ElfLEhReloc(s))
            continue;
        ElfLCodigo cod = ElfLConferirSecaoReloc(s, erro);
        if (cod != ELFL_OK)
            return cod;
        *total += (size_t)(s->tam / s->entsize);
    }
    return ELFL_OK;
}

/* r_info = simbolo (32 bits altos) e tipo (32 baixos). SHT_REL nao tem addend: o valor fica no byte alvo. */
static void ElfLLerRelocEntrada(const uint8_t *e, uint32_t tipo_secao, ElfLRelocacao *r)
{
    uint64_t info = ElfLU64(e + 8);
    r->off = ElfLU64(e);
    r->simbolo = (uint32_t)(info >> 32);
    r->tipo = (uint32_t)(info & 0xffffffffu);
    r->addend = tipo_secao == ELFL_SHT_RELA ? (int64_t)ElfLU64(e + 16) : 0;
}

static size_t ElfLIndiceCenso(uint32_t tipo)
{
    for (size_t i = 0; i < ELFL_TAM_CENSO; i++) {
        if (ELFL_CENSO[i].tipo == tipo)
            return i;
    }
    return ELFL_TAM_CENSO;
}

static bool ElfLNoCenso(uint32_t tipo)
{
    return ElfLIndiceCenso(tipo) < ELFL_TAM_CENSO;
}

static ElfLCodigo ElfLRelocFora(ElfLErro *erro, uint32_t tipo, const char *secao)
{
    erro->codigo = ELFL_RELOC_FORA_DO_CENSO;
    erro->tipo_reloc = tipo;
    snprintf(erro->mensagem, sizeof erro->mensagem, "relocacao de tipo %u (%s) na secao %s fora do censo", tipo, ElfLNomeReloc(tipo), secao);
    return ELFL_RELOC_FORA_DO_CENSO;
}

/* Preenche todas as relocacoes. O 1o tipo fora do censo vira erro, mas a leitura segue ate o fim: a contagem do membro sai inteira. */
static ElfLCodigo ElfLPreencherRelocacoes(const uint8_t *bytes, ElfLObjeto *obj, ElfLErro *erro)
{
    size_t k = 0;
    ElfLCodigo resultado = ELFL_OK;
    for (size_t i = 0; i < obj->n_secoes; i++) {
        const ElfLSecao *s = &obj->secoes[i];
        if (!ElfLEhReloc(s))
            continue;
        size_t quantas = (size_t)(s->tam / s->entsize);
        for (size_t j = 0; j < quantas; j++, k++) {
            ElfLRelocacao *r = &obj->relocacoes[k];
            ElfLLerRelocEntrada(bytes + s->off + j * s->entsize, s->tipo, r);
            if (resultado == ELFL_OK && !ElfLNoCenso(r->tipo))
                resultado = ElfLRelocFora(erro, r->tipo, s->nome);
        }
    }
    return resultado;
}

static ElfLCodigo ElfLLerRelocacoes(const uint8_t *bytes, ElfLObjeto *obj, ElfLErro *erro)
{
    size_t total = 0;
    ElfLCodigo cod = ElfLContarRelocacoes(obj, &total, erro);
    if (cod != ELFL_OK)
        return cod;
    obj->n_relocacoes = total;
    obj->relocacoes = ElfLAlocar(total, sizeof *obj->relocacoes);
    if (obj->relocacoes == NULL)
        return ElfLSemMemoria(erro);
    return ElfLPreencherRelocacoes(bytes, obj, erro);
}

ElfLCodigo ElfLer(const uint8_t *bytes, size_t n, ElfLObjeto *obj, ElfLErro *erro)
{
    ElfLCodigo cod;
    memset(obj, 0, sizeof *obj);
    erro->codigo = ELFL_OK;
    erro->tipo_reloc = 0;
    erro->mensagem[0] = '\0';
    cod = ElfLConferirEhdr(bytes, n, erro);
    if (cod == ELFL_OK)
        cod = ElfLLerSecoes(bytes, n, obj, erro);
    if (cod == ELFL_OK)
        cod = ElfLNomearSecoes(bytes, obj, erro);
    if (cod == ELFL_OK)
        cod = ElfLLerSimbolos(bytes, obj, erro);
    if (cod == ELFL_OK)
        cod = ElfLLerRelocacoes(bytes, obj, erro);
    return cod;
}

void ElfLLiberar(ElfLObjeto *obj)
{
    free(obj->secoes);
    free(obj->simbolos);
    free(obj->relocacoes);
    memset(obj, 0, sizeof *obj);
}

const char *ElfLNomeReloc(uint32_t tipo)
{
    size_t i = ElfLIndiceCenso(tipo);
    return i < ELFL_TAM_CENSO ? ELFL_CENSO[i].nome : "?";
}

/* ---------- ponte para o Objeto neutro (M5b P35) ---------- */

#define ELFL_SHF_WRITE 0x1u
#define ELFL_SHF_ALLOC 0x2u
#define ELFL_SHF_EXECINSTR 0x4u
#define ELFL_SHF_TLS 0x400u
#define ELFL_SHT_NULL 0u
#define ELFL_SHT_NOTE 7u
#define ELFL_SHT_INIT_ARRAY 14u
#define ELFL_SHT_FINI_ARRAY 15u
#define ELFL_SHT_GROUP 17u
#define ELFL_SHT_UNWIND 0x70000001u
#define ELFL_STB_LOCAL 0u
#define ELFL_STB_GLOBAL 1u
#define ELFL_STB_WEAK 2u
#define ELFL_STT_SECTION 3u
#define ELFL_STT_FILE 4u
#define ELFL_SHN_UNDEF 0u
#define ELFL_SHN_LORESERVE 0xff00u
#define ELFL_SECAO_DESCARTADA 0xffffffffu
#define ELFL_SIMBOLO_FORA (-1)
#define ELFL_R_64 1u
#define ELFL_R_PC32 2u
#define ELFL_R_PLT32 4u
#define ELFL_R_32 10u
#define ELFL_R_32S 11u
#define ELFL_R_GOTPCREL 9u
#define ELFL_R_GOTPCRELX 41u
#define ELFL_R_REX_GOTPCRELX 42u
#define ELFL_R_TPOFF32 23u
#define ELFL_R_GOTTPOFF 22u

/* Estado da conversao: o leitor cru ja feito (raw), o Objeto de destino e, por indice ELF, o indice no Objeto. */
typedef struct {
    const uint8_t *bytes;
    ElfLObjeto *raw;
    Objeto *o;
    ElfLErro *erro;
    uint32_t *secao_obj;   /* por secao ELF: indice no Objeto, ou ELFL_SECAO_DESCARTADA */
    int64_t *simbolo_obj;  /* por simbolo ELF: indice no Objeto, ou ELFL_SIMBOLO_FORA */
} ElfLConversao;

/* Secoes que o ligador nativo nao carrega: as de depuracao e de notas, o COMDAT .group (a deduplicacao entre membros e do P36) e .eh_frame (o unwind do ELF). */
static bool ElfLSecaoDescartada(const ElfLSecao *s)
{
    if (s->tipo == ELFL_SHT_NULL || s->tipo == ELFL_SHT_SYMTAB || s->tipo == ELFL_SHT_STRTAB || ElfLEhReloc(s))
        return true;
    if (s->tipo == ELFL_SHT_NOTE || s->tipo == ELFL_SHT_GROUP || s->tipo == ELFL_SHT_UNWIND)
        return true;
    if ((s->flags & ELFL_SHF_ALLOC) == 0)
        return true;
    return strcmp(s->nome, ".eh_frame") == 0;
}

static ObjTipoSecao ElfLTipoSecao(const ElfLSecao *s)
{
    if (s->tipo == ELFL_SHT_INIT_ARRAY)
        return OS_INIT_ARRAY;
    if ((s->flags & ELFL_SHF_TLS) != 0)
        return s->tipo == ELFL_SHT_NOBITS ? OS_TBSS : OS_TDATA;
    if ((s->flags & ELFL_SHF_EXECINSTR) != 0)
        return OS_TEXTO;
    if (s->tipo == ELFL_SHT_NOBITS)
        return OS_BSS;
    if ((s->flags & ELFL_SHF_WRITE) != 0)
        return OS_DATA;
    return OS_RDATA;
}

/* O Objeto so guarda alinhamento potencia de 2 (0 e 1 viram 1, como no ELF). */
static ElfLCodigo ElfLAlinhamento(ElfLErro *erro, const ElfLSecao *s, uint32_t *alinha)
{
    uint64_t a = s->alinhamento <= 1 ? 1 : s->alinhamento;
    if ((a & (a - 1)) != 0 || a > 0x10000u)
        return ElfLFalha(erro, "alinhamento de secao nao e potencia de 2 ou passa de 65536", s->off);
    *alinha = (uint32_t)a;
    return ELFL_OK;
}

/* Copia o conteudo da secao ELF para a secao nova do Objeto (BSS so guarda o tamanho virtual). */
static ElfLCodigo ElfLPreencherSecao(ElfLConversao *c, const ElfLSecao *s, uint32_t idx)
{
    ObjSecao *os = VecEm(&c->o->secoes, idx);
    if (s->tam > 0xffffffffu)
        return ElfLFalha(c->erro, "secao maior que 4 GiB", s->off);
    if (s->tipo == ELFL_SHT_NOBITS) {
        os->tam_bss = (uint32_t)s->tam;
        return ELFL_OK;
    }
    if (s->tam > 0)
        BufAnexarN(&os->bytes, (const char *)(c->bytes + s->off), (size_t)s->tam);
    return ELFL_OK;
}

static ElfLCodigo ElfLAbrirSecao(ElfLConversao *c, size_t k)
{
    const ElfLSecao *s = &c->raw->secoes[k];
    uint32_t alinha = 1;
    ElfLCodigo cod;
    if (ElfLSecaoDescartada(s)) {
        c->secao_obj[k] = ELFL_SECAO_DESCARTADA;
        return ELFL_OK;
    }
    if (s->tipo == ELFL_SHT_FINI_ARRAY)
        return ElfLFalha(c->erro, "secao .fini_array ainda nao tratada pelo ligador nativo", s->off);
    cod = ElfLAlinhamento(c->erro, s, &alinha);
    if (cod != ELFL_OK)
        return cod;
    c->secao_obj[k] = ObjSecaoAbrir(c->o, s->nome, ElfLTipoSecao(s), alinha);
    return ElfLPreencherSecao(c, s, c->secao_obj[k]);
}

static ElfLCodigo ElfLAbrirSecoes(ElfLConversao *c)
{
    for (size_t k = 0; k < c->raw->n_secoes; k++) {
        ElfLCodigo cod = ElfLAbrirSecao(c, k);
        if (cod != ELFL_OK)
            return cod;
    }
    return ELFL_OK;
}

static void ElfLMarcarFraco(Objeto *o, uint32_t idx, uint8_t bind)
{
    if (bind == ELFL_STB_WEAK)
        ((ObjSimbolo *)VecEm(&o->simbolos, idx))->fraco = true;
}

/* Simbolo de secao (STT_SECTION, sem nome): vira ".secao<k>" na posicao 0 da secao. Secao descartada nao tem simbolo (a relocacao contra ele falha depois). */
static void ElfLSimboloSecao(ElfLConversao *c, size_t i)
{
    uint16_t sec = c->raw->simbolos[i].secao;
    char nome[32];
    if (sec >= c->raw->n_secoes || c->secao_obj[sec] == ELFL_SECAO_DESCARTADA)
        return;
    snprintf(nome, sizeof nome, ".secao%u", (unsigned)sec);
    c->simbolo_obj[i] = ObjSimboloDefinir(c->o, nome, (int32_t)c->secao_obj[sec], 0);
}

static ElfLCodigo ElfLSimboloIndefinido(ElfLConversao *c, size_t i, uint8_t bind)
{
    const ElfLSimbolo *s = &c->raw->simbolos[i];
    uint32_t idx;
    if (bind == ELFL_STB_LOCAL)
        return ElfLFalha(c->erro, "simbolo local indefinido (o offset e o indice da symtab)", (uint64_t)i);
    idx = ObjSimboloReferir(c->o, s->nome);
    ElfLMarcarFraco(c->o, idx, bind);
    c->simbolo_obj[i] = idx;
    return ELFL_OK;
}

static bool ElfLJaDefinido(const Objeto *o, const char *nome)
{
    int64_t j = ObjAcharSimbolo(o, nome);
    return j >= 0 && ((const ObjSimbolo *)VecEm(&o->simbolos, (size_t)j))->secao != OBJ_SECAO_INDEFINIDA;
}

/* Local: nome ".<nome>#<indice>" (o ponto o põe fora do global e o indice o torna unico no membro). */
static void ElfLSimboloLocal(ElfLConversao *c, size_t i, uint32_t secao)
{
    const ElfLSimbolo *s = &c->raw->simbolos[i];
    Buf nome = {0};
    BufFormatar(&nome, ".%s#%zu", s->nome, i);
    c->simbolo_obj[i] = ObjSimboloDefinir(c->o, nome.dados, (int32_t)secao, (uint32_t)s->valor);
    BufLiberar(&nome);
}

static ElfLCodigo ElfLSimboloDefinido(ElfLConversao *c, size_t i)
{
    const ElfLSimbolo *s = &c->raw->simbolos[i];
    uint8_t bind = (uint8_t)(s->info >> 4);
    uint32_t secao = c->secao_obj[s->secao];
    if (bind == ELFL_STB_LOCAL) {
        ElfLSimboloLocal(c, i, secao);
        return ELFL_OK;
    }
    if (ElfLJaDefinido(c->o, s->nome))
        return ElfLFalha(c->erro, "simbolo global definido duas vezes no mesmo membro", (uint64_t)i);
    c->simbolo_obj[i] = ObjSimboloDefinir(c->o, s->nome, (int32_t)secao, (uint32_t)s->valor);
    ElfLMarcarFraco(c->o, (uint32_t)c->simbolo_obj[i], bind);
    return ELFL_OK;
}

static ElfLCodigo ElfLSimboloUm(ElfLConversao *c, size_t i)
{
    const ElfLSimbolo *s = &c->raw->simbolos[i];
    uint8_t tipo = (uint8_t)(s->info & 0xfu);
    uint8_t bind = (uint8_t)(s->info >> 4);
    if (tipo == ELFL_STT_FILE)
        return ELFL_OK;
    if (tipo == ELFL_STT_SECTION) {
        ElfLSimboloSecao(c, i);
        return ELFL_OK;
    }
    if (s->nome[0] == '\0')
        return ELFL_OK;
    if (s->secao == ELFL_SHN_UNDEF)
        return ElfLSimboloIndefinido(c, i, bind);
    if (s->secao >= ELFL_SHN_LORESERVE)
        return ElfLFalha(c->erro, "simbolo absoluto, comum ou reservado ainda nao tratado pelo ligador nativo", (uint64_t)i);
    if (s->secao >= c->raw->n_secoes)
        return ElfLFalha(c->erro, "simbolo com secao fora da tabela de secoes", (uint64_t)i);
    if (c->secao_obj[s->secao] == ELFL_SECAO_DESCARTADA)
        return ELFL_OK;
    return ElfLSimboloDefinido(c, i);
}

static ElfLCodigo ElfLSimbolos(ElfLConversao *c)
{
    for (size_t i = 0; i < c->raw->n_simbolos; i++) {
        ElfLCodigo cod;
        c->simbolo_obj[i] = ELFL_SIMBOLO_FORA;
        cod = ElfLSimboloUm(c, i);
        if (cod != ELFL_OK)
            return cod;
    }
    return ELFL_OK;
}

/* Tipo do censo para o neutro (R_X86_64_PLT32 vira OR_REL32: sem PLT no link estatico; o P36 decide o stub). Tipo fora daqui nao chega aqui (ElfLer recusa). */
static bool ElfLTipoNeutro(uint32_t tipo, ObjTipoReloc *out)
{
    switch (tipo) {
    case ELFL_R_64: *out = OR_ADDR64; return true;
    case ELFL_R_PC32: case ELFL_R_PLT32: *out = OR_REL32; return true;
    case ELFL_R_32: *out = OR_ABS32; return true;
    case ELFL_R_32S: *out = OR_ABS32S; return true;
    case ELFL_R_GOTPCREL: *out = OR_GOTPCREL; return true;
    case ELFL_R_GOTPCRELX: case ELFL_R_REX_GOTPCRELX: *out = OR_GOTPCRELX; return true;
    case ELFL_R_TPOFF32: *out = OR_TPOFF32; return true;
    case ELFL_R_GOTTPOFF: *out = OR_GOTTPOFF; return true;
    default: return false;
    }
}

/* Campo que a relocacao corrige: 8 bytes so no ADDR64; o resto e 4. */
static uint64_t ElfLTamanhoCampo(ObjTipoReloc tipo)
{
    return tipo == OR_ADDR64 ? 8u : 4u;
}

static ElfLCodigo ElfLRelocUm(ElfLConversao *c, const ElfLRelocacao *r, uint32_t secao)
{
    ObjTipoReloc tipo;
    const ObjSecao *os = VecEm(&c->o->secoes, secao);
    int64_t sym = r->simbolo < c->raw->n_simbolos ? c->simbolo_obj[r->simbolo] : ELFL_SIMBOLO_FORA;
    if (!ElfLTipoNeutro(r->tipo, &tipo))
        return ElfLRelocFora(c->erro, r->tipo, os->nome);
    if (sym == ELFL_SIMBOLO_FORA)
        return ElfLFalha(c->erro, "relocacao contra simbolo sem equivalente no ligador (nulo, sem nome ou de secao descartada)", r->off);
    if (r->off + ElfLTamanhoCampo(tipo) > os->bytes.tam)
        return ElfLFalha(c->erro, "relocacao com campo fora da secao", r->off);
    ObjRelocar(c->o, (ObjAlvoReloc){secao, (uint32_t)r->off}, (ObjCorrecao){tipo, (uint32_t)sym, r->addend});
    return ELFL_OK;
}

/* Relocacoes de uma secao RELA: *k anda pelas entradas do leitor cru (na ordem das secoes), mesmo quando a secao alvo e descartada. */
static ElfLCodigo ElfLRelocsDeSecao(ElfLConversao *c, size_t i, size_t *k)
{
    const ElfLSecao *s = &c->raw->secoes[i];
    size_t quantas = (size_t)(s->tam / s->entsize);
    uint32_t alvo = s->info;
    if (alvo >= c->raw->n_secoes)
        return ElfLFalha(c->erro, "secao de relocacao aponta para secao fora da tabela", s->off);
    if (c->secao_obj[alvo] == ELFL_SECAO_DESCARTADA) {
        *k += quantas;
        return ELFL_OK;
    }
    if (s->tipo == ELFL_SHT_REL)
        return ElfLFalha(c->erro, "secao de relocacao REL (addend no byte) ainda nao tratada pelo ligador nativo", s->off);
    for (size_t j = 0; j < quantas; j++, (*k)++) {
        ElfLCodigo cod = ElfLRelocUm(c, &c->raw->relocacoes[*k], c->secao_obj[alvo]);
        if (cod != ELFL_OK)
            return cod;
    }
    return ELFL_OK;
}

static ElfLCodigo ElfLRelocacoes(ElfLConversao *c)
{
    size_t k = 0;
    for (size_t i = 0; i < c->raw->n_secoes; i++) {
        ElfLCodigo cod;
        if (!ElfLEhReloc(&c->raw->secoes[i]))
            continue;
        cod = ElfLRelocsDeSecao(c, i, &k);
        if (cod != ELFL_OK)
            return cod;
    }
    return ELFL_OK;
}

static ElfLCodigo ElfLConverter(ElfLConversao *c)
{
    ElfLCodigo cod = ElfLAbrirSecoes(c);
    if (cod == ELFL_OK)
        cod = ElfLSimbolos(c);
    if (cod == ELFL_OK)
        cod = ElfLRelocacoes(c);
    return cod;
}

ElfLCodigo ElfLerObjeto(const uint8_t *bytes, size_t n, Objeto *o, ElfLErro *erro)
{
    ElfLObjeto raw;
    ElfLCodigo cod = ElfLer(bytes, n, &raw, erro);
    ElfLConversao c = {bytes, &raw, o, erro, NULL, NULL};
    if (cod != ELFL_OK) {
        ElfLLiberar(&raw);
        return cod;
    }
    c.secao_obj = ElfLAlocar(raw.n_secoes, sizeof *c.secao_obj);
    c.simbolo_obj = ElfLAlocar(raw.n_simbolos, sizeof *c.simbolo_obj);
    cod = (c.secao_obj == NULL || c.simbolo_obj == NULL) ? ElfLSemMemoria(erro) : ElfLConverter(&c);
    free(c.secao_obj);
    free(c.simbolo_obj);
    ElfLLiberar(&raw);
    return cod;
}

/* Indefinido GLOBAL de verdade: secao 0, ligacao GLOBAL (fraco nao puxa membro, como o COFF), nome nao vazio. */
static bool ElfLEhIndefinidoGlobal(const ElfLSimbolo *s)
{
    uint8_t tipo = (uint8_t)(s->info & 0xfu);
    uint8_t bind = (uint8_t)(s->info >> 4);
    return s->secao == ELFL_SHN_UNDEF && bind == ELFL_STB_GLOBAL && tipo != ELFL_STT_SECTION && tipo != ELFL_STT_FILE && s->nome[0] != '\0';
}

ElfLCodigo ElfListarIndefinidos(const uint8_t *bytes, size_t n, const ElfLDestinoNomes *destino, ElfLErro *erro)
{
    ElfLObjeto raw;
    ElfLCodigo cod = ElfLer(bytes, n, &raw, erro);
    if (cod == ELFL_RELOC_FORA_DO_CENSO) {
        erro->codigo = ELFL_OK;
        cod = ELFL_OK;
    }
    for (size_t i = 0; cod == ELFL_OK && i < raw.n_simbolos; i++) {
        if (ElfLEhIndefinidoGlobal(&raw.simbolos[i]))
            *(const char **)VecEmpurrar(destino->lista) = ArenaCopiarTexto(destino->arena, raw.simbolos[i].nome, strlen(raw.simbolos[i].nome));
    }
    ElfLLiberar(&raw);
    return cod;
}
