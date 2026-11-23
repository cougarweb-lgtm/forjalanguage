/* teste_common_spec.c - verificacoes de unidade da doc embutida, do mapa codigo -> secao e do campo "spec" (SpecVerificacoes, chamada pelo teste_common): IA1-b09.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: 100% dos codigos do catalogo com secao no mapa que existe na spec e no cartao e com trecho; secoes por tema; texto do spec do E0502 e do E0707 (com exemplo
 *            certo); exemplos.txt (E0707 com conserto, codigo sem exemplo); JSON de diagnostico sem sugestao leva spec e depois spec_ref.
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe
 */
#include <stdio.h>
#include <string.h>

#include "common/diag.h"
#include "common/diag_spec.h"
#include "common/doc_embutida.h"
#include "common/fonte.h"
#include "common/spec_doc.h"
#include "common/spec_exemplos.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok) return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (spec) linha %d: %s\n", linha, texto);
}

#define SPEC_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

static const char *const TEMAS[] = {"formato", "declarar", "proc", "fluxo", "erro", "memoria", "colecoes", "texto", "foreign", "modulos", "testes", "saida", "cli"};
enum { QTD_TEMAS = sizeof TEMAS / sizeof TEMAS[0] };

/* Os 13 temas, na mesma ordem, no cartao e na spec; os 5 arquivos embutidos existem e terminam em NUL. */
static void TestaDocEmbutida(const Contagem *c)
{
    SpecSecao spec[SPEC_SECOES_MAX], cartao[SPEC_SECOES_MAX];
    size_t nspec = SpecSecoes(DOC_SPEC, spec, SPEC_SECOES_MAX);
    size_t ncartao = SpecSecoes(DOC_CARTAO, cartao, SPEC_SECOES_MAX);
    bool temas = nspec == QTD_TEMAS && ncartao == QTD_TEMAS;

    for (size_t k = 0; temas && k < QTD_TEMAS; k++) temas = strcmp(spec[k].nome, TEMAS[k]) == 0 && strcmp(cartao[k].nome, TEMAS[k]) == 0;
    SPEC_VERIFICA(c, temas);
    for (int k = 0; k < DOC_QTD; k++) {
        DocTexto t = DocEmbutida((DocArquivo)k);

        SPEC_VERIFICA(c, t.tam > 0 && t.texto[t.tam] == '\0');
    }
    SPEC_VERIFICA(c, memcmp(DocEmbutida(DOC_CARTAO).texto, "CARTAO FORJA", 12) == 0);
}

/* 100% dos codigos do catalogo: secao no mapa, secao existente na spec e no cartao, trecho nao vazio e curto. */
static void TestaMapaCompleto(const Contagem *c)
{
    SpecSecao spec[SPEC_SECOES_MAX], cartao[SPEC_SECOES_MAX];
    size_t nspec = SpecSecoes(DOC_SPEC, spec, SPEC_SECOES_MAX);
    size_t ncartao = SpecSecoes(DOC_CARTAO, cartao, SPEC_SECOES_MAX);
    int sem_mapa = 0, sem_secao = 0, sem_trecho = 0;

    for (int k = 0; k < DIAG_QTD; k++) {
        const char *codigo = DiagCodigoTexto((DiagCodigo)k);
        char secao[SPEC_NOME_MAX];
        char trecho[SPEC_TRECHO_MAX + 8];

        if (!SpecSecaoDoCodigo(codigo, secao)) {
            sem_mapa++;
            continue;
        }
        if (SpecAcharSecao(spec, nspec, secao) == NULL || SpecAcharSecao(cartao, ncartao, secao) == NULL) sem_secao++;
        size_t n = SpecTrechoDoCodigo(codigo, secao, trecho, sizeof trecho);
        if (n == 0 || n > SPEC_TRECHO_MAX + 3 || strlen(trecho) != n) sem_trecho++;
    }
    SPEC_VERIFICA(c, sem_mapa == 0 && sem_secao == 0 && sem_trecho == 0);
    SPEC_VERIFICA(c, !SpecSecaoDoCodigo("E9999", (char[SPEC_NOME_MAX]){0}) && !SpecSecaoDoCodigo("E050", (char[SPEC_NOME_MAX]){0}));
}

static void TestaTextoDoSpec(const Contagem *c)
{
    char texto[DIAG_SPEC_MAX];
    SpecExemplo e;

    DiagSpecTexto("E0502", texto, sizeof texto);
    SPEC_VERIFICA(c, strncmp(texto, "proc: ", 6) == 0 && strstr(texto, "E0502") != NULL);
    DiagSpecTexto("E0707", texto, sizeof texto);
    SPEC_VERIFICA(c, strncmp(texto, "declarar: ", 10) == 0 && strstr(texto, "E0707") != NULL && strstr(texto, "\nexemplo certo:\n") != NULL && strstr(texto, "let ") != NULL);
    DiagSpecTexto("G0001", texto, sizeof texto);
    SPEC_VERIFICA(c, strncmp(texto, "foreign: ", 9) == 0 && strstr(texto, "\nexemplo certo:") == NULL); /* codigo nao citado na spec: o comeco da secao */
    SPEC_VERIFICA(c, SpecExemploDoCodigo("E0707", &e) && e.certo.tam > 0 && e.errado.tam > 0 && e.linha > 0 && e.primeira > 0 && e.primeira <= e.linha);
    SPEC_VERIFICA(c, !SpecExemploDoCodigo("R0001", &e) && !SpecExemploDoCodigo("E0", &e));
}

/* Imprime o diagnostico (ou, com explicar, o catalogo do codigo dele) num arquivo temporario e le a linha de volta. */
static void LerSaida(char *linha, size_t cap, const Diag *d, const Fonte *f, bool explicar)
{
    FILE *arquivo_temporario = tmpfile();
    size_t lidos;

    if (explicar) DiagExplicarJson(d->codigo, arquivo_temporario);
    else DiagImprimirJson(d, f, arquivo_temporario);
    rewind(arquivo_temporario);
    lidos = fread(linha, 1, cap - 1, arquivo_temporario);
    fclose(arquivo_temporario);
    linha[lidos] = '\0';
}

/* Diagnostico sem sugestao: o spec sai no fim da linha na 1a vez e vira spec_ref na seguinte. */
static void TestaSpecNoJson(const Contagem *c)
{
    Fonte f = FonteDeTexto("a.fj", "proc main() {\n    print(1)\n}\n");
    Diag d = {0};
    char linha[8192];
    DiagRelatar(&d, E0308, 18, "teste do spec");
    DiagSpecIniciarLinha(E0308);
    DiagSpecIniciarLinha(E0308); /* garante que o codigo ja foi visto: a linha seguinte so leva o spec_ref */
    LerSaida(linha, sizeof linha, &d, &f, false);
    SPEC_VERIFICA(c, strstr(linha, "\"spec_ref\": \"forja explicar E0308\"") != NULL && strstr(linha, "\"spec\": ") == NULL);
    LerSaida(linha, sizeof linha, &d, &f, true);
    SPEC_VERIFICA(c, strstr(linha, "\"spec\": \"colecoes: ") != NULL && strstr(linha, "\"spec_ref\"") == NULL);
}

void SpecVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    TestaDocEmbutida(&c);
    TestaMapaCompleto(&c);
    TestaTextoDoSpec(&c);
    TestaSpecNoJson(&c);
}
