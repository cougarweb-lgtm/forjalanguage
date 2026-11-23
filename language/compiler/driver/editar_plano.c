/* editar_plano.c - o plano de edicao do 'forja editar': arquivos tocados, edicoes de cada um e as operacoes pedidas (ver editar_interno.h).
 * Versao: 0.2.1
 * Changelog: 0.2.1 - G3 G0c: CompararEdicoes desempata por coluna_fim, linha_fim, antigo e novo (ordem total).
 *            0.2.0 - IA1-I5: PlanoTrocarLocais (faixa por offsets locais ao arquivo).
 *            0.1.0 - IA1-I4: primeira versao.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache/entradas_ler.h"
#include "driver/editar_interno.h"

bool PlanoErro(Plano *p, const char *fmt, ...)
{
    va_list args;

    if (p->erro[0] != '\0') return false;
    va_start(args, fmt);
    vsnprintf(p->erro, sizeof p->erro, fmt, args);
    va_end(args);
    return false;
}

/* ---------- arquivos ---------- */

static bool CaminhoEhDaStd(const char *caminho)
{
    return strncmp(caminho, "std:", 4) == 0;
}

/* O arquivo do plano (lido na 1a vez que e tocado). NULL = erro em p->erro (std embutida ou arquivo ilegivel). */
static ArquivoEditado *AcharArquivo(Plano *p, const char *caminho)
{
    ArquivoEditado *a;
    char relativo[EDITAR_CAMINHO_MAX];

    for (uint32_t k = 0; k < p->narquivos; k++) {
        if (strcmp(p->arquivos[k].caminho, caminho) == 0) return &p->arquivos[k];
    }
    if (CaminhoEhDaStd(caminho)) {
        PlanoErro(p, "'%s' e da biblioteca padrao embutida no forja: nao se edita", caminho);
        return NULL;
    }
    if (p->narquivos == p->cap) {
        p->cap = p->cap == 0 ? 4 : p->cap * 2;
        p->arquivos = realloc(p->arquivos, p->cap * sizeof *p->arquivos);
        if (p->arquivos == NULL) FalhaInterna("memoria esgotada guardando os arquivos do plano de edicao");
    }
    a = &p->arquivos[p->narquivos];
    *a = (ArquivoEditado){.caminho = ArenaCopiarTexto(p->arena, caminho, strlen(caminho))};
    EditarCaminhoRelativo(p, caminho, relativo, sizeof relativo);
    a->relativo = ArenaCopiarTexto(p->arena, relativo, strlen(relativo));
    if (!ArquivoLerInteiro(caminho, &a->original, &a->tam)) {
        PlanoErro(p, "nao consegui ler '%s'", caminho);
        return NULL;
    }
    p->narquivos++;
    return a;
}

/* O texto do arquivo sem o BOM (as posicoes das edicoes nao o contam). */
static const char *TextoDe(const ArquivoEditado *a, size_t *tam)
{
    size_t bom = EdicoesTemBom(a->original, a->tam) ? EDICOES_BOM_TAM : 0;

    *tam = a->tam - bom;
    return a->original + bom;
}

/* Offset de (linha, coluna) no texto; SIZE_MAX se a linha nao existe. */
static size_t OffsetDe(const char *texto, size_t tam, uint32_t linha, uint32_t coluna)
{
    size_t ini = 0;

    for (uint32_t l = 1; l < linha; l++) {
        const char *nl = memchr(texto + ini, '\n', tam - ini);

        if (nl == NULL) return SIZE_MAX;
        ini = (size_t)(nl - texto) + 1;
    }
    return ini + coluna - 1 <= tam ? ini + coluna - 1 : SIZE_MAX;
}

/* ---------- novo texto no fim de linha do arquivo ---------- */

/* O arquivo usa CRLF? (o 1o fim de linha decide) */
static bool UsaCrlf(const char *texto, size_t tam)
{
    const char *nl = memchr(texto, '\n', tam);

    return nl != NULL && nl > texto && nl[-1] == '\r';
}

/* 'novo' com cada LF puro trocado por CRLF (so em arquivo CRLF). */
static const char *NovoNoFimDeLinha(Plano *p, const char *novo, bool crlf)
{
    Buf b = {0};
    char *copia;

    if (!crlf || strchr(novo, '\n') == NULL) return ArenaCopiarTexto(p->arena, novo, strlen(novo));
    for (const char *c = novo; *c != '\0'; c++) {
        if (*c == '\n' && (c == novo || c[-1] != '\r')) BufAnexarN(&b, "\r", 1);
        BufAnexarN(&b, c, 1);
    }
    copia = ArenaCopiarTexto(p->arena, b.dados, b.tam);
    BufLiberar(&b);
    return copia;
}

/* ---------- edicoes ---------- */

static void EmpurrarEdicao(ArquivoEditado *a, const Edicao *e)
{
    if (a->n == a->cap) {
        a->cap = a->cap == 0 ? 8 : a->cap * 2;
        a->edicoes = realloc(a->edicoes, a->cap * sizeof *a->edicoes);
        if (a->edicoes == NULL) FalhaInterna("memoria esgotada guardando as edicoes do plano");
    }
    a->edicoes[a->n++] = *e;
}

static bool JaTem(const ArquivoEditado *a, const Edicao *e)
{
    for (uint32_t k = 0; k < a->n; k++)
        if (EdicoesIguais(&a->edicoes[k], e)) return true;
    return false;
}

/* Tira o que a faixa e o texto novo tem igual no comeco e no fim: o diff de uma troca de varias linhas so mostra o que mudou.
 * LIMITE: parametros 5 (texto, o par de offsets e o par novo+tamanho se alteram juntos; sao 3 saidas de uma mesma aparagem). */
static void AparaIguais(const char *texto, size_t *de, size_t *ate, const char **novo, size_t *tam_novo)
{
    size_t velho = *ate - *de, ini = 0, fim = 0;

    while (ini < velho && ini < *tam_novo && texto[*de + ini] == (*novo)[ini]) ini++;
    while (fim < velho - ini && fim < *tam_novo - ini && texto[*ate - 1 - fim] == (*novo)[*tam_novo - 1 - fim]) fim++;
    *de += ini;
    *ate -= fim;
    *novo += ini;
    *tam_novo -= ini + fim;
}

/* Linha e coluna (a partir de 1) do offset no texto. */
static void PosicaoDe(const char *texto, size_t off, uint32_t *linha, uint32_t *coluna)
{
    size_t ini_linha = 0;

    *linha = 1;
    for (size_t k = 0; k < off; k++) {
        if (texto[k] != '\n') continue;
        (*linha)++;
        ini_linha = k + 1;
    }
    *coluna = (uint32_t)(off - ini_linha) + 1;
}

/* Edicao de [de, ate) por novo[0..tam_novo), com as posicoes recalculadas dos offsets (depois de aparar).
 * LIMITE: parametros 6 (texto+de+ate delimitam a faixa e novo+tam_novo o texto novo; sao os mesmos pares de buffer da fronteira do motor de edicoes). */
static Edicao EdicaoDeOffsets(Plano *p, const char *texto, size_t de, size_t ate, const char *novo, size_t tam_novo)
{
    Edicao e = {0};

    PosicaoDe(texto, de, &e.linha, &e.coluna);
    PosicaoDe(texto, ate, &e.linha_fim, &e.coluna_fim);
    if (e.linha_fim == e.linha) e.linha_fim = 0;
    e.antigo = ArenaCopiarTexto(p->arena, texto + de, ate - de);
    e.novo = ArenaCopiarTexto(p->arena, novo, tam_novo);
    return e;
}

bool PlanoTrocarFaixa(Plano *p, const char *caminho, const Edicao *faixa, const char *novo)
{
    ArquivoEditado *a = AcharArquivo(p, caminho);
    size_t tam, de, ate, tam_novo;
    const char *texto;
    Edicao e;

    if (a == NULL) return false;
    texto = TextoDe(a, &tam);
    de = OffsetDe(texto, tam, faixa->linha, faixa->coluna);
    ate = OffsetDe(texto, tam, faixa->linha_fim > faixa->linha ? faixa->linha_fim : faixa->linha, faixa->coluna_fim);
    if (de == SIZE_MAX || ate == SIZE_MAX || ate < de) return PlanoErro(p, "posicao fora do arquivo '%s' (linha %u, coluna %u)", caminho, faixa->linha, faixa->coluna);
    novo = NovoNoFimDeLinha(p, novo, UsaCrlf(texto, tam));
    tam_novo = strlen(novo);
    if (faixa->linha_fim > faixa->linha) {
        AparaIguais(texto, &de, &ate, &novo, &tam_novo);
        if (de == ate && tam_novo == 0) return true;
        e = EdicaoDeOffsets(p, texto, de, ate, novo, tam_novo);
    } else {
        e = *faixa;
        e.antigo = ArenaCopiarTexto(p->arena, texto + de, ate - de);
        e.novo = ArenaCopiarTexto(p->arena, novo, tam_novo);
    }
    if (!JaTem(a, &e)) EmpurrarEdicao(a, &e);
    return true;
}

bool PlanoTrocarOffsets(Plano *p, uint32_t ini, uint32_t fim, const char *novo)
{
    const Fonte *f = &p->q->c->fonte;
    FontePosicao de = FonteLocalizarNoArquivo(f, ini);
    FontePosicao ate = FonteLocalizarNoArquivo(f, fim);
    Edicao faixa;

    if (fim > ini && (strcmp(de.caminho, ate.caminho) != 0 || ate.linha < de.linha)) {
        /* 'fim' cai no separador entre modulos: o fim e o byte anterior mais um */
        ate = FonteLocalizarNoArquivo(f, fim - 1);
        ate.coluna++;
    }
    faixa = (Edicao){.linha = de.linha, .coluna = de.coluna, .linha_fim = ate.linha, .coluna_fim = ate.coluna};
    if (strcmp(de.caminho, ate.caminho) != 0) return PlanoErro(p, "a faixa a editar atravessa dois arquivos");
    return PlanoTrocarFaixa(p, de.caminho, &faixa, novo);
}

/* LIMITE: parametros 5 (o plano e o arquivo dizem onde; o par de offsets locais e o texto novo dizem o que trocar; mesma forma do PlanoTrocarFaixa, que ele estende). */
bool PlanoTrocarLocais(Plano *p, const char *caminho, uint32_t ini, uint32_t fim, const char *novo)
{
    ArquivoEditado *a = AcharArquivo(p, caminho);
    Edicao faixa = {0};
    const char *texto;
    size_t tam;

    if (a == NULL) return false;
    texto = TextoDe(a, &tam);
    if (ini > fim || fim > tam) return PlanoErro(p, "posicao fora do arquivo '%s' (offset %u)", caminho, fim);
    PosicaoDe(texto, ini, &faixa.linha, &faixa.coluna);
    PosicaoDe(texto, fim, &faixa.linha_fim, &faixa.coluna_fim);
    return PlanoTrocarFaixa(p, caminho, &faixa, novo);
}

uint32_t PlanoTotalEdicoes(const Plano *p)
{
    uint32_t total = 0;

    for (uint32_t k = 0; k < p->narquivos; k++) total += p->arquivos[k].n;
    return total;
}

/* LIMITE: parametros 5 (os quatro campos de uma linha do registro mais o plano; agrupar em struct so criaria um tipo de uso unico). */
void PlanoRegistrar(Plano *p, const char *op, const char *alvo, const char *novo, uint32_t edicoes_antes)
{
    p->operacoes = realloc(p->operacoes, ((size_t)p->noperacoes + 1) * sizeof *p->operacoes);
    if (p->operacoes == NULL) FalhaInterna("memoria esgotada guardando as operacoes do plano");
    p->operacoes[p->noperacoes++] = (Operacao){.op = op, .alvo = alvo, .novo = novo, .edicoes = PlanoTotalEdicoes(p) - edicoes_antes};
}

/* ---------- ordem, liberar ---------- */

static int CompararEdicoes(const void *x, const void *y)
{
    const Edicao *a = x, *b = y;

    if (a->linha != b->linha) return a->linha < b->linha ? -1 : 1;
    if (a->coluna != b->coluna) return a->coluna < b->coluna ? -1 : 1;
    if (a->coluna_fim != b->coluna_fim) return a->coluna_fim < b->coluna_fim ? -1 : 1;
    if (a->linha_fim != b->linha_fim) return a->linha_fim < b->linha_fim ? -1 : 1;
    if (strcmp(a->antigo, b->antigo) != 0) return strcmp(a->antigo, b->antigo) < 0 ? -1 : 1;
    return strcmp(a->novo, b->novo); /* desempate pelo conteudo: o array guarda valores, sem indice de insercao */
}

static int CompararArquivos(const void *x, const void *y)
{
    const ArquivoEditado *a = x, *b = y;

    return strcmp(a->relativo, b->relativo);
}

void PlanoOrdenar(Plano *p)
{
    qsort(p->arquivos, p->narquivos, sizeof *p->arquivos, CompararArquivos);
    for (uint32_t k = 0; k < p->narquivos; k++) qsort(p->arquivos[k].edicoes, p->arquivos[k].n, sizeof(Edicao), CompararEdicoes);
}

void PlanoLiberar(Plano *p)
{
    for (uint32_t k = 0; k < p->narquivos; k++) {
        free(p->arquivos[k].original);
        free(p->arquivos[k].edicoes);
    }
    free(p->arquivos);
    free(p->operacoes);
    if (p->sintatica != NULL) {
        LiberarCompilacao(p->sintatica);
        free(p->sintatica);
    }
    *p = (Plano){0};
}
