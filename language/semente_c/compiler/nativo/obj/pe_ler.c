/* pe_ler.c - leitor de PE de terceiro (DLL ou exe x64): a tabela de exportacoes por nome (P19a do M3).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M3 F9b-1: decisoes extraidas de PelAbrir e PeLerExportacoes.
 *            0.1.0 - M3 P19a: PeLerExportacoes. Todo acesso passa por PelFaixa (RVA -> bytes de uma secao, dentro de n): PE truncado ou com RVA solto
 *                    vira X0105, nunca leitura fora do buffer. Forwarder = RVA da funcao dentro do diretorio de exportacao.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <string.h>

#include "nativo/obj/pe_ler.h"

#define PEL_OFF_LFANEW 0x3Cu
#define PEL_TAM_DOS 0x40u
#define PEL_OFF_NSECOES 6u
#define PEL_OFF_TAM_OPCIONAL 20u
#define PEL_OFF_OPCIONAL 24u
#define PEL_OFF_MAQUINA 4u
#define PEL_MAGIC_PE32 0x10Bu
#define PEL_MAGIC_PE32_MAIS 0x20Bu
#define PEL_MAQUINA_AMD64 0x8664u
#define PEL_OFF_QTD_DIRETORIOS 108u
#define PEL_OFF_DIRETORIOS 112u
#define PEL_TAM_DIRETORIO 8u
#define PEL_TAM_SECAO 40u
#define PEL_OFF_SECAO_TAM_VIRTUAL 8u
#define PEL_OFF_SECAO_RVA 12u
#define PEL_OFF_SECAO_BRUTO 16u
#define PEL_OFF_SECAO_PTR 20u
#define PEL_TAM_DIR_EXPORT 40u
#define PEL_OFF_EXP_BASE 16u
#define PEL_OFF_EXP_QTD_FUNCOES 20u
#define PEL_OFF_EXP_QTD_NOMES 24u
#define PEL_OFF_EXP_FUNCOES 28u
#define PEL_OFF_EXP_NOMES 32u
#define PEL_OFF_EXP_ORDINAIS 36u
#define PEL_TAM_RVA 4u
#define PEL_TAM_ORDINAL 2u

/* As tres tabelas do diretorio de exportacao: nomes (RVA de texto), ordinais (indice em funcoes) e funcoes (RVA). */
typedef struct {
    const uint8_t *nomes, *ordinais, *funcoes;
    uint32_t qtd_nomes, qtd_funcoes, base;
} PelTabelas;

typedef struct {
    const uint8_t *b;
    size_t n;
    size_t secoes;            /* offset da tabela de secoes */
    uint32_t qtd_secoes;
    uint32_t dir_rva, dir_tam;  /* diretorio de exportacao (0 = nao tem) */
    PelTabelas t;
    PeExportacoes *saida;
    Diag *erro;
} PeLeitor;

static uint16_t PelLe16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t PelLe32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool PelCabe(const PeLeitor *L, size_t off, size_t len)
{
    return off <= L->n && len <= L->n - off;
}

static bool PelMalformado(const PeLeitor *L, const char *motivo)
{
    return DiagRelatar(L->erro, X0105, 0, "PE malformado: %s", motivo);
}

/* Os len bytes que comecam no RVA, dentro dos dados gravados de UMA secao e do buffer; NULL se nao couberem. */
static const uint8_t *PelFaixa(const PeLeitor *L, uint32_t rva, size_t len)
{
    for (uint32_t i = 0; i < L->qtd_secoes; i++) {
        const uint8_t *s = L->b + L->secoes + (size_t)i * PEL_TAM_SECAO;
        uint32_t sec_rva = PelLe32(s + PEL_OFF_SECAO_RVA), bruto = PelLe32(s + PEL_OFF_SECAO_BRUTO);
        if (rva < sec_rva || rva - sec_rva >= bruto)
            continue;
        size_t off = (size_t)PelLe32(s + PEL_OFF_SECAO_PTR) + (rva - sec_rva);
        return PelCabe(L, off, len) ? L->b + off : NULL;
    }
    return NULL;
}

/* Texto terminado em NUL que comeca no RVA, inteiro dentro do buffer; NULL se nao ha. */
static const char *PelTexto(const PeLeitor *L, uint32_t rva)
{
    const uint8_t *p = PelFaixa(L, rva, 1);
    if (p == NULL || memchr(p, 0, (size_t)(L->b + L->n - p)) == NULL)
        return NULL;
    return (const char *)p;
}

/* Magic e maquina: so PE32+ AMD64 passa; PE32 (32 bits) e a causa conhecida do X0105. */
static bool PelConferirOpcional(const PeLeitor *L, size_t pe)
{
    uint16_t magic = PelLe16(L->b + pe + PEL_OFF_OPCIONAL);
    uint16_t maquina = PelLe16(L->b + pe + PEL_OFF_MAQUINA);
    if (magic == PEL_MAGIC_PE32)
        return DiagRelatar(L->erro, X0105, 0, "biblioteca de 32 bits (PE32); o backend nativo liga so x64");
    if (magic != PEL_MAGIC_PE32_MAIS)
        return PelMalformado(L, "Magic do cabecalho opcional desconhecido");
    if (maquina != PEL_MAQUINA_AMD64)
        return DiagRelatar(L->erro, X0105, 0, "biblioteca para a maquina 0x%x; o backend nativo liga so x64 (0x8664)", maquina);
    return true;
}

/* Tabela de secoes e entrada 0 (exportacao) dos diretorios de dados. */
static bool PelGuardarTabelas(PeLeitor *L, size_t pe)
{
    size_t opcional = pe + PEL_OFF_OPCIONAL;
    uint16_t tam_opcional = PelLe16(L->b + pe + PEL_OFF_TAM_OPCIONAL);
    L->qtd_secoes = PelLe16(L->b + pe + PEL_OFF_NSECOES);
    L->secoes = opcional + tam_opcional;
    if (!PelCabe(L, L->secoes, (size_t)L->qtd_secoes * PEL_TAM_SECAO) || !PelCabe(L, opcional, tam_opcional))
        return PelMalformado(L, "tabela de secoes truncada");
    if (tam_opcional < PEL_OFF_DIRETORIOS + PEL_TAM_DIRETORIO || PelLe32(L->b + opcional + PEL_OFF_QTD_DIRETORIOS) == 0)
        return true;
    L->dir_rva = PelLe32(L->b + opcional + PEL_OFF_DIRETORIOS);
    L->dir_tam = PelLe32(L->b + opcional + PEL_OFF_DIRETORIOS + PEL_TAM_RVA);
    return true;
}

static bool PelTemAssinaturaMz(const PeLeitor *L)
{
    return PelCabe(L, 0, PEL_TAM_DOS) && L->b[0] == 'M' && L->b[1] == 'Z';
}

static bool PelAbrir(PeLeitor *L)
{
    if (!PelTemAssinaturaMz(L))
        return PelMalformado(L, "sem assinatura MZ");
    size_t pe = PelLe32(L->b + PEL_OFF_LFANEW);
    if (!PelCabe(L, pe, PEL_OFF_OPCIONAL + PEL_TAM_ORDINAL) || memcmp(L->b + pe, "PE\0\0", 4) != 0)
        return PelMalformado(L, "sem assinatura PE");
    return PelConferirOpcional(L, pe) && PelGuardarTabelas(L, pe);
}

/* Le o diretorio de exportacao e confere que as tres tabelas cabem no arquivo. Sem nomes, as tabelas ficam vazias. */
static bool PelAbrirTabelas(PeLeitor *L)
{
    const uint8_t *d = PelFaixa(L, L->dir_rva, PEL_TAM_DIR_EXPORT);
    if (d == NULL)
        return PelMalformado(L, "diretorio de exportacao fora do arquivo");
    PelTabelas *t = &L->t;
    t->base = PelLe32(d + PEL_OFF_EXP_BASE);
    t->qtd_funcoes = PelLe32(d + PEL_OFF_EXP_QTD_FUNCOES);
    t->qtd_nomes = PelLe32(d + PEL_OFF_EXP_QTD_NOMES);
    if (t->qtd_nomes == 0)
        return true;
    t->nomes = PelFaixa(L, PelLe32(d + PEL_OFF_EXP_NOMES), (size_t)t->qtd_nomes * PEL_TAM_RVA);
    t->ordinais = PelFaixa(L, PelLe32(d + PEL_OFF_EXP_ORDINAIS), (size_t)t->qtd_nomes * PEL_TAM_ORDINAL);
    t->funcoes = PelFaixa(L, PelLe32(d + PEL_OFF_EXP_FUNCOES), (size_t)t->qtd_funcoes * PEL_TAM_RVA);
    if (t->nomes == NULL || t->ordinais == NULL || t->funcoes == NULL)
        return PelMalformado(L, "tabela de exportacao fora do arquivo");
    return true;
}

/* RVA dentro do proprio diretorio de exportacao = texto "dll.simbolo" (a DLL repassa o simbolo a outra). */
static const char *PelForwarder(const PeLeitor *L, uint32_t rva)
{
    if (rva < L->dir_rva || rva - L->dir_rva >= L->dir_tam)
        return NULL;
    const char *texto = PelTexto(L, rva);
    return texto == NULL ? NULL : ArenaCopiarTexto(&L->saida->textos, texto, strlen(texto));
}

static bool PelLerItem(PeLeitor *L, uint32_t i)
{
    const PelTabelas *t = &L->t;
    uint32_t indice = PelLe16(t->ordinais + (size_t)i * PEL_TAM_ORDINAL);
    const char *nome = PelTexto(L, PelLe32(t->nomes + (size_t)i * PEL_TAM_RVA));
    if (nome == NULL || indice >= t->qtd_funcoes)
        return PelMalformado(L, "nome ou ordinal da tabela de exportacao fora do arquivo");
    uint32_t rva = PelLe32(t->funcoes + (size_t)indice * PEL_TAM_RVA);
    PeExportacao *e = VecEmpurrar(&L->saida->itens);
    e->nome = ArenaCopiarTexto(&L->saida->textos, nome, strlen(nome));
    e->ordinal = t->base + indice;
    e->rva = rva;
    e->forwarder = PelForwarder(L, rva);
    return true;
}

static bool PelLerItens(PeLeitor *L)
{
    for (uint32_t i = 0; i < L->t.qtd_nomes; i++) {
        if (!PelLerItem(L, i))
            return false;
    }
    return true;
}

bool PeLerExportacoes(const uint8_t *bytes, size_t n, PeExportacoes *saida, Diag *erro)
{
    PeLeitor L = {.b = bytes, .n = n, .saida = saida, .erro = erro};
    saida->textos = (Arena){0};
    VecIniciar(&saida->itens, sizeof(PeExportacao));
    if (!PelAbrir(&L))
        return false;
    if (L.dir_rva == 0 || L.dir_tam == 0)
        return true;
    if (!PelAbrirTabelas(&L))
        return false;
    return PelLerItens(&L);
}

void PeExportacoesLiberar(PeExportacoes *saida)
{
    VecLiberar(&saida->itens);
    ArenaLiberar(&saida->textos);
}
