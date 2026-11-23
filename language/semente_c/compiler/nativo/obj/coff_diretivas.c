/* coff_diretivas.c - le o .drectve de um membro COFF de terceiros: /DEFAULTLIB e -l (biblioteca implicita), /EXPORT e -export (so na DLL), -aligncomm (alinhamento de simbolo comum).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M3b P23y-2: -aligncomm com valor que nao cabe em 32 bits (4294967300 virava 4) deixa de ser lido e vira diretiva IGNORADA (o aviso do ligador a cita).
 *            0.1.0 - M3b P23e: CoffDiretivasLer (texto -> CoffDiretiva) e CoffBibliotecasDoMembro (as bibliotecas implicitas de um membro, para o resolvedor de 'link' achar antes de ligar).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <ctype.h>
#include <string.h>

#include "nativo/obj/coff_interno.h"
#include "nativo/obj/coff_membro.h"

#define DIR_NOME_DIRETIVA ".drectve"
#define DIR_BASE_DECIMAL 10u

typedef struct {
    const char *prefixo;
    CoffTipoDiretiva tipo;
} DirPrefixo;

static const DirPrefixo DIR_PREFIXOS[] = {
    {"defaultlib:", DIRETIVA_BIBLIOTECA},
    {"export:", DIRETIVA_EXPORTA},
    {"aligncomm:", DIRETIVA_ALINHA_COMUM},
};

static bool DirBranco(char c)
{
    return c == '\0' || isspace((unsigned char)c);
}

static size_t DirPularBrancos(const char *t, size_t n, size_t pos)
{
    while (pos < n && DirBranco(t[pos]))
        pos++;
    return pos;
}

/* Um token acaba no 1o branco fora de aspas ("nome com espaco"). */
static size_t DirFimDoToken(const char *t, size_t n, size_t pos)
{
    bool aspas = false;

    for (; pos < n && (aspas || !DirBranco(t[pos])); pos++) {
        if (t[pos] == '"')
            aspas = !aspas;
    }
    return pos;
}

static bool DirIgual(const char *a, const char *b, size_t n)
{
    for (size_t k = 0; k < n; k++) {
        if (tolower((unsigned char)a[k]) != tolower((unsigned char)b[k]))
            return false;
    }
    return true;
}

/* Copia len caracteres para a arena, sem as aspas. */
static const char *DirNome(Arena *arena, const char *p, size_t len)
{
    char *copia = ArenaAlocar(arena, len + 1);
    size_t m = 0;

    for (size_t k = 0; k < len; k++) {
        if (p[k] != '"')
            copia[m++] = p[k];
    }
    copia[m] = '\0';
    return copia;
}

static void DirAnexar(Vec *saida, CoffTipoDiretiva tipo, const char *nome, uint32_t valor)
{
    *(CoffDiretiva *)VecEmpurrar(saida) = (CoffDiretiva){tipo, nome, valor};
}

/* Falso se o numero nao cabe em uint32_t (4294967300 nao pode virar 4). */
static bool DirNumero(const char *p, size_t len, uint32_t *valor)
{
    uint64_t v = 0;

    for (size_t k = 0; k < len && p[k] >= '0' && p[k] <= '9'; k++) {
        v = v * DIR_BASE_DECIMAL + (uint64_t)(p[k] - '0');
        if (v > UINT32_MAX)
            return false;
    }
    *valor = (uint32_t)v;
    return true;
}

/* "nome,valor" (aligncomm) ou "nome[,resto]" (export): o nome vai ate a 1a virgula fora de aspas, que aqui e a 1a virgula. */
static size_t DirTamNome(const char *p, size_t len)
{
    const char *virgula = memchr(p, ',', len);

    return virgula != NULL ? (size_t)(virgula - p) : len;
}

/* Um trecho do texto (o que vem depois do prefixo da diretiva). */
typedef struct {
    const char *p;
    size_t len;
} DirTrecho;

static bool DirAnexarPorTipo(Arena *arena, CoffTipoDiretiva tipo, const DirTrecho *t, Vec *saida)
{
    size_t nome = DirTamNome(t->p, t->len);
    uint32_t valor = 0;

    switch (tipo) {
    case DIRETIVA_ALINHA_COMUM:
        if (nome == t->len || !DirNumero(t->p + nome + 1, t->len - nome - 1, &valor))
            return false;  /* sem valor ou valor que nao cabe: vira IGNORADA e o aviso do ligador cita a diretiva */
        DirAnexar(saida, tipo, DirNome(arena, t->p, nome), valor);
        return true;
    case DIRETIVA_BIBLIOTECA:
        DirAnexar(saida, tipo, DirNome(arena, t->p, t->len), 0);
        return true;
    case DIRETIVA_EXPORTA:
        DirAnexar(saida, tipo, DirNome(arena, t->p, nome), 0);
        return true;
    case DIRETIVA_IGNORADA:
        break;
    }
    return false;
}

static bool DirConhecida(Arena *arena, const char *tok, size_t len, Vec *saida)
{
    for (size_t k = 0; k < sizeof DIR_PREFIXOS / sizeof DIR_PREFIXOS[0]; k++) {
        size_t p = strlen(DIR_PREFIXOS[k].prefixo);
        DirTrecho resto = {tok + p, len - p};

        if (len >= p && DirIgual(tok, DIR_PREFIXOS[k].prefixo, p))
            return DirAnexarPorTipo(arena, DIR_PREFIXOS[k].tipo, &resto, saida);
    }
    return false;
}

/* O '/' ou '-' que abre a diretiva; '\0' se o token nao comeca por um dos dois. */
static char DirMarca(const char *tok, size_t len)
{
    if (len > 1 && (tok[0] == '/' || tok[0] == '-'))
        return tok[0];
    return '\0';
}

/* So o "-l<nome>" do gcc vale com '-' (o '/' do MSVC nunca e -l). */
static void DirTratar(Arena *arena, const char *tok, size_t len, Vec *saida)
{
    char marca = DirMarca(tok, len);

    if (marca == '\0' || !DirConhecida(arena, tok + 1, len - 1, saida)) {
        if (marca == '-' && len > 2 && tok[1] == 'l')
            DirAnexar(saida, DIRETIVA_BIBLIOTECA, DirNome(arena, tok + 2, len - 2), 0);
        else
            DirAnexar(saida, DIRETIVA_IGNORADA, DirNome(arena, tok, len), 0);
    }
}

void CoffDiretivasLer(const char *texto, size_t n, Arena *arena, Vec *saida)
{
    size_t pos = DirPularBrancos(texto, n, 0);

    while (pos < n) {
        size_t fim = DirFimDoToken(texto, n, pos);

        DirTratar(arena, texto + pos, fim - pos, saida);
        pos = DirPularBrancos(texto, n, fim);
    }
}

/* As BIBLIOTECA do texto da secao entram em saida como const char *. */
static void DirAnotarBibliotecas(const char *texto, size_t n, Arena *arena, Vec *saida)
{
    Vec itens;

    VecIniciar(&itens, sizeof(CoffDiretiva));
    CoffDiretivasLer(texto, n, arena, &itens);
    for (size_t k = 0; k < itens.n; k++) {
        const CoffDiretiva *d = VecEm(&itens, k);

        if (d->tipo == DIRETIVA_BIBLIOTECA)
            *(const char **)VecEmpurrar(saida) = d->nome;
    }
    VecLiberar(&itens);
}

static bool DirSecaoDiretiva(const CoffLeitor *L, uint32_t i, Arena *arena, Vec *saida)
{
    const uint8_t *h = CoffCabecalhoSecao(L, i);
    char buf[COFF_TAM_NOME + 1];
    const char *nome = CoffNomeSecao(L, h, buf);
    size_t ptr = (size_t)CoffLe(h + COFF_OFF_SEC_PTR, COFF_TAM_CAMPO);
    size_t raw = (size_t)CoffLe(h + COFF_OFF_SEC_RAW, COFF_TAM_CAMPO);

    if (nome == NULL)
        return CoffFalha(L->erro, "nome de secao fora da tabela de strings");
    if (strcmp(nome, DIR_NOME_DIRETIVA) != 0)
        return true;
    if (!CoffCabe(L->n, ptr, raw))
        return CoffFalha(L->erro, "dados da secao .drectve fora do arquivo");
    DirAnotarBibliotecas((const char *)L->b + ptr, raw, arena, saida);
    return true;
}

bool CoffBibliotecasDoMembro(const uint8_t *b, size_t n, const CoffDestinoNomes *destino, Diag *erro)
{
    CoffLeitor L = {b, n, NULL, erro, 0, 0, 0, 0, 0, 0, {0}};

    if (!CoffLerCabecalho(&L))
        return false;
    CoffLerTabelaStrings(&L);
    for (uint32_t i = 0; i < L.nsec; i++) {
        if (!DirSecaoDiretiva(&L, i, destino->arena, destino->lista))
            return false;
    }
    return true;
}
