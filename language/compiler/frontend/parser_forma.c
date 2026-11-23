/* parser_forma.c - forma por ocorrencia do E0101 (contrato IA1, D-IA1-5, passo b07): a explicacao e a forma_certa dizem o que o parser esperava NAQUELE ponto,
 * com o token encontrado, o token de antes, a linha, a coluna e o nome do arquivo (o catalogo guarda so o texto geral do codigo).
 * Duas entradas: ParserFormaEsperado (o ErroSintaxe, que sabe o que esperava) e ParserFormaDaRegra (o E0101 de regra fixa, "import so vale no topo...", que usa a
 * propria causa como regra); as duas escrevem em p->diag com DiagForma, depois do DiagRelatar.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b07: primeira versao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_forma_b07.py --forja compiler\forja.exe
 */
#include "frontend/parser_interno.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* TEXTO_LOCAL: o buffer onde a frase e montada inteira (cabe o pior caso de campos cheios); DiagForma corta em DIAG_FORMA_MAX - 1. */
enum { PEDACO_MAX = 56, VARREDURA_MAX = 200, ONDE_MAX = 160, TEXTO_LOCAL = 1024 };

/* O ponto do erro. achado e anterior ja saem entre aspas ('x'), ou como "o fim da linha" / "o fim do arquivo" / "" (comeco do comando).
 * onde = "linha 2, coluna 9 de a.fj". */
typedef struct {
    char achado[PEDACO_MAX], anterior[PEDACO_MAX], onde[ONDE_MAX];
    uint32_t linha, coluna_abridor;
    TokenTipo tipo;
    char abridor; /* o '(' '[' ou '{' ainda aberto quando se esperava o fechamento; 0 = nao se esperava fechamento ou nao achei */
} Ocorrencia;

static const struct { const char *esperado; TokenTipo abre, fecha; char abridor; } FECHOS[] = {
    {"')'", TK_LPAREN, TK_RPAREN, '('}, {"']'", TK_LCOLCHETE, TK_RCOLCHETE, '['}, {"'}'", TK_LBRACE, TK_RBRACE, '{'},
};

static void Anexar(char *saida, const char *fmt, ...) __attribute__((format(gnu_printf, 2, 3)));

/* Escreve no fim do texto que ja esta em saida (o buffer tem TEXTO_LOCAL bytes; o que nao cabe e cortado). */
static void Anexar(char *saida, const char *fmt, ...)
{
    size_t usado = strlen(saida);
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(saida + usado, TEXTO_LOCAL - usado, fmt, ap);
    va_end(ap);
}

/* Binario: o ultimo token que comeca em off ou antes. */
static uint32_t IndiceDoToken(const Parser *p, uint32_t off)
{
    uint32_t ini = 0, fim = (uint32_t)p->tokens->n;

    while (fim - ini > 1) {
        uint32_t meio = ini + (fim - ini) / 2;
        if (p->toks[meio].off <= off) ini = meio;
        else fim = meio;
    }
    return ini;
}

static void TokenEntreAspas(const Parser *p, const Token *t, char *destino)
{
    size_t tam = t->len < PEDACO_MAX - 3 ? t->len : PEDACO_MAX - 3;

    if (t->tipo == TK_NL) snprintf(destino, PEDACO_MAX, "o fim da linha");
    else if (t->tipo == TK_EOF) snprintf(destino, PEDACO_MAX, "o fim do arquivo");
    else snprintf(destino, PEDACO_MAX, "'%.*s'", (int)tam, p->fonte->texto + t->off);
}

/* Posicao em FECHOS do fechamento esperado ("')'"), ou -1 se o esperado nao e um fechamento. */
static int IndiceDoFecho(const char *esperado)
{
    for (size_t f = 0; f < sizeof FECHOS / sizeof FECHOS[0]; f++) {
        if (strcmp(esperado, FECHOS[f].esperado) == 0) return (int)f;
    }
    return -1;
}

/* Procura para tras, no mesmo comando, o abridor que o fechamento esperado deixaria em aberto; para em quebra de linha (menos para '}'). */
static void LocalizarAbridor(const Parser *p, uint32_t indice, const char *esperado, Ocorrencia *o)
{
    int f = IndiceDoFecho(esperado);
    int profundidade = 0;

    if (f < 0) return;
    for (uint32_t k = indice; k > 0 && indice - k < VARREDURA_MAX; k--) {
        const Token *t = &p->toks[k - 1];
        if (t->tipo == TK_NL && FECHOS[f].abre != TK_LBRACE) return;
        if (t->tipo == FECHOS[f].fecha) {
            profundidade++;
        } else if (t->tipo == FECHOS[f].abre && profundidade-- == 0) {
            o->abridor = FECHOS[f].abridor;
            o->coluna_abridor = FonteLocalizarNoArquivo(p->fonte, t->off).coluna;
            return;
        }
    }
}

static void LerOcorrencia(const Parser *p, uint32_t indice, Ocorrencia *o)
{
    memset(o, 0, sizeof *o);
    o->tipo = (TokenTipo)p->toks[indice].tipo;
    o->linha = FonteLocalizarNoArquivo(p->fonte, p->toks[indice].off).linha;
    DiagDescreverOnde(p->fonte, p->toks[indice].off, o->onde, sizeof o->onde);
    TokenEntreAspas(p, &p->toks[indice], o->achado);
    if (indice > 0 && p->toks[indice - 1].tipo != TK_NL) TokenEntreAspas(p, &p->toks[indice - 1], o->anterior);
}

/* " depois de 'x'" ou "" no comeco do comando. */
static void DepoisDe(const Ocorrencia *o, char *saida)
{
    if (o->anterior[0] != '\0') snprintf(saida, PEDACO_MAX + 16, " depois de %s", o->anterior);
    else saida[0] = '\0';
}

static void ExplicarEsperado(const Ocorrencia *o, const char *esperado, char *saida)
{
    char depois[PEDACO_MAX + 16];

    DepoisDe(o, depois);
    if (o->tipo == TK_NL) snprintf(saida, TEXTO_LOCAL, "Na %s o comando termina%s, mas o parser ainda esperava %s.", o->onde, depois, esperado);
    else if (o->tipo == TK_EOF) snprintf(saida, TEXTO_LOCAL, "O arquivo acaba na %s%s, mas o parser ainda esperava %s.", o->onde, depois, esperado);
    else snprintf(saida, TEXTO_LOCAL, "Na %s o parser esperava %s%s e encontrou %s.", o->onde, esperado, depois, o->achado);
    if (o->abridor != '\0') Anexar(saida, " O '%c' da coluna %u continua aberto.", o->abridor, o->coluna_abridor);
}

/* O fechamento ()]} esperado: com o abridor achado, diz qual abridor fecha. */
static void FormaDeFechamento(const Ocorrencia *o, const char *esperado, char *saida)
{
    char depois[PEDACO_MAX + 16];
    bool fim = o->tipo == TK_NL || o->tipo == TK_EOF;

    DepoisDe(o, depois);
    if (o->abridor != '\0' && fim) snprintf(saida, TEXTO_LOCAL, "Feche o '%c' da coluna %u com %s antes do fim da linha.", o->abridor, o->coluna_abridor, esperado);
    else if (o->abridor != '\0')
        snprintf(saida, TEXTO_LOCAL, "Escreva %s logo%s, para fechar o '%c' da coluna %u; %s vem depois do fechamento.", esperado, depois, o->abridor, o->coluna_abridor, o->achado);
    else if (fim) snprintf(saida, TEXTO_LOCAL, "Termine a linha com %s.", esperado);
    else snprintf(saida, TEXTO_LOCAL, "Escreva %s%s, antes de %s.", esperado, depois, o->achado);
}

static void FormaDoEsperado(const Ocorrencia *o, const char *esperado, char *saida)
{
    char depois[PEDACO_MAX + 16];
    bool fim = o->tipo == TK_NL || o->tipo == TK_EOF;

    DepoisDe(o, depois);
    if (IndiceDoFecho(esperado) >= 0) FormaDeFechamento(o, esperado, saida);
    else if (strcmp(esperado, "expressao") == 0 && fim) snprintf(saida, TEXTO_LOCAL, "Complete a linha com um valor, um nome ou uma conta%s.", depois);
    else if (strcmp(esperado, "expressao") == 0)
        snprintf(saida, TEXTO_LOCAL, "Escreva um valor, um nome ou uma conta%s; %s nao comeca uma expressao.", depois, o->achado);
    else if (strcmp(esperado, "fim de linha") == 0) snprintf(saida, TEXTO_LOCAL, "Um comando por linha: quebre a linha antes de %s.", o->achado);
    else if (o->tipo == TK_NL) snprintf(saida, TEXTO_LOCAL, "Complete a linha %u com %s%s.", o->linha, esperado, depois);
    else if (fim) snprintf(saida, TEXTO_LOCAL, "Complete o arquivo com %s%s.", esperado, depois);
    else snprintf(saida, TEXTO_LOCAL, "Escreva %s%s no lugar de %s.", esperado, depois, o->achado);
}

void ParserFormaEsperado(Parser *p, const char *esperado)
{
    Ocorrencia o;
    char cabeca[PEDACO_MAX * 4], explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];
    const char *exemplo = strstr(esperado, ": ");

    /* "limite da fatia: a[lo..<hi]": o exemplo fica so na causa (o parser o escreveu para um caso e a forma da ocorrencia nao carrega nome que o arquivo nao tem). */
    snprintf(cabeca, sizeof cabeca, "%.*s", exemplo != NULL ? (int)(exemplo - esperado) : (int)strlen(esperado), esperado);
    LerOcorrencia(p, p->cur, &o);
    LocalizarAbridor(p, p->cur, cabeca, &o);
    ExplicarEsperado(&o, cabeca, explicacao);
    FormaDoEsperado(&o, cabeca, forma);
    DiagForma(p->diag, explicacao, forma);
}

void ParserFormaDaRegra(Parser *p)
{
    Ocorrencia o;
    char depois[PEDACO_MAX + 16];
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];
    const char *causa = p->diag->causa;
    const char *regra = strstr(causa, ": ");

    if (!p->diag->tem || p->diag->codigo != E0101 || p->diag->explicacao[0] != '\0') return;
    LerOcorrencia(p, IndiceDoToken(p, p->diag->off), &o);
    DepoisDe(&o, depois);
    snprintf(explicacao, sizeof explicacao, "Na %s o parser parou em %s%s porque a regra da Forja neste ponto e: %.130s.", o.onde, o.achado, depois, causa);
    if (regra != NULL) snprintf(forma, sizeof forma, "Ajuste a linha %u conforme a regra: %.150s.", o.linha, regra + 2);
    else snprintf(forma, sizeof forma, "Reescreva a linha %u sem %s nesse ponto, obedecendo a regra: %.130s.", o.linha, o.achado, causa);
    DiagForma(p->diag, explicacao, forma);
}
