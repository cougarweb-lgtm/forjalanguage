/* teste_common_edicoes.c - verificacoes de unidade do motor de edicoes e do leitor de JSON do driver (EdicoesVerificacoes, chamada pelo teste_common): IA1-b05.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I4: +4 verificacoes de edicao multilinha (linha_fim: juntar linhas, quebrar, CRLF, antigo que nao bate); inicializadores com o campo linha_fim.
 *            0.1.0 - IA1-b05: EdicoesAplicar (troca, insercao, varias na mesma linha, CRLF, ultima linha sem LF, antigo que nao bate, faixa fora, linha inexistente, sobreposicao,
 *                    insercao no mesmo ponto, 'novo' com quebra de linha), EdicoesDiff (hunk simples, dois hunks, marca de fim sem LF, sem edicao), EdicoesLinhaNova e JsonLer
 *                    (objeto, lista, escapes, \u, par substituto, invalidos).
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe
 */
#include <stdio.h>
#include <string.h>

#include "common/arena.h"
#include "common/buf.h"
#include "driver/edicoes.h"
#include "driver/json_leitor.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok) return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (edicoes) linha %d: %s\n", linha, texto);
}

#define EDICOES_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* Aplica e compara com o esperado; devolve o status. */
static EdicaoStatus Aplicar(const char *texto, const Edicao *lista, uint32_t n, Buf *saida, uint32_t *ruim)
{
    return EdicoesAplicar(texto, strlen(texto), lista, n, saida, ruim);
}

static bool Resultado(const char *texto, const Edicao *lista, uint32_t n, const char *esperado)
{
    Buf b = {0};
    bool ok = Aplicar(texto, lista, n, &b, NULL) == EDICAO_OK && b.tam == strlen(esperado) && (b.tam == 0 || memcmp(b.dados, esperado, b.tam) == 0);

    BufLiberar(&b);
    return ok;
}

static void TestaAplicar(const Contagem *c)
{
    const Edicao troca = {2, 5, 8, "var", "let", 0};
    const Edicao duas[] = {{1, 3, 4, "b", "BB", 0}, {1, 1, 1, "", ">", 0}};
    const Edicao multilinha = {1, 2, 3, "b", "x\ny", 0};

    EDICOES_VERIFICA(c, Resultado("a\n    var n\nz\n", &troca, 1, "a\n    let n\nz\n"));
    EDICOES_VERIFICA(c, Resultado("a b c\n", duas, 2, ">a BB c\n"));
    EDICOES_VERIFICA(c, Resultado("a\r\n    var n\r\nz", &troca, 1, "a\r\n    let n\r\nz"));
    EDICOES_VERIFICA(c, Resultado("abc", &multilinha, 1, "ax\nyc"));
    EDICOES_VERIFICA(c, Resultado("igual\n", NULL, 0, "igual\n"));
}

static void TestaRecusas(const Contagem *c)
{
    const Edicao antigo_errado = {1, 1, 4, "let", "var", 0};
    const Edicao fora = {1, 1, 9, "var n", "x", 0};
    const Edicao sem_linha = {3, 1, 1, "", "x", 0};
    const Edicao coluna_zero = {1, 0, 1, "v", "x", 0};
    const Edicao no_cr = {1, 5, 6, "\r", "x", 0};
    const Edicao sobrepostas[] = {{1, 1, 4, "var", "let", 0}, {1, 3, 5, "r ", "x", 0}};
    const Edicao mesmo_ponto[] = {{1, 1, 1, "", "a", 0}, {1, 1, 1, "", "b", 0}};
    Buf b = {0};
    uint32_t ruim = 99;

    EDICOES_VERIFICA(c, Aplicar("var n\n", &antigo_errado, 1, &b, &ruim) == EDICAO_ANTIGO_DIFERE && ruim == 0);
    EDICOES_VERIFICA(c, Aplicar("var n\n", &fora, 1, &b, NULL) == EDICAO_FAIXA_INVALIDA);
    EDICOES_VERIFICA(c, Aplicar("var n\n", &sem_linha, 1, &b, NULL) == EDICAO_SEM_LINHA);
    EDICOES_VERIFICA(c, Aplicar("var n\n", &coluna_zero, 1, &b, NULL) == EDICAO_FAIXA_INVALIDA);
    EDICOES_VERIFICA(c, Aplicar("var \r\n", &no_cr, 1, &b, NULL) == EDICAO_FAIXA_INVALIDA);
    EDICOES_VERIFICA(c, Aplicar("var n\n", sobrepostas, 2, &b, &ruim) == EDICAO_SOBREPOSTA && ruim == 1);
    EDICOES_VERIFICA(c, Aplicar("var n\n", mesmo_ponto, 2, &b, NULL) == EDICAO_SOBREPOSTA);
    EDICOES_VERIFICA(c, b.tam == 0);
    BufLiberar(&b);
}

static bool DiffIgual(const char *texto, const Edicao *lista, uint32_t n, const char *esperado)
{
    Buf b = {0};
    bool ok = EdicoesDiff(texto, strlen(texto), lista, n, "a.fj", &b) == EDICAO_OK && b.tam == strlen(esperado) && (b.tam == 0 || memcmp(b.dados, esperado, b.tam) == 0);

    if (!ok && b.dados != NULL) fprintf(stderr, "diff obtido:\n%s\n", b.dados);
    BufLiberar(&b);
    return ok;
}

static void TestaDiff(const Contagem *c)
{
    const Edicao um = {5, 1, 4, "var", "let", 0};
    const Edicao dois[] = {{1, 1, 4, "var", "let", 0}, {12, 1, 4, "var", "let", 0}};
    const Edicao quebra = {2, 2, 3, "b", "b\nc", 0};
    const char *doze = "var\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\nvar\n";

    EDICOES_VERIFICA(c, DiffIgual("1\n2\n3\n4\nvar\n6\n7\n8\n9\n", &um, 1,
                                  "--- a/a.fj\n+++ b/a.fj\n@@ -2,7 +2,7 @@\n 2\n 3\n 4\n-var\n+let\n 6\n 7\n 8\n"));
    EDICOES_VERIFICA(c, DiffIgual(doze, dois, 2,
                                  "--- a/a.fj\n+++ b/a.fj\n@@ -1,4 +1,4 @@\n-var\n+let\n 2\n 3\n 4\n@@ -9,4 +9,4 @@\n 9\n 10\n 11\n-var\n+let\n"));
    EDICOES_VERIFICA(c, DiffIgual("a\nab\nz\n", &quebra, 1, "--- a/a.fj\n+++ b/a.fj\n@@ -1,3 +1,4 @@\n a\n-ab\n+ab\n+c\n z\n"));
    EDICOES_VERIFICA(c, DiffIgual("var", &(Edicao){1, 1, 4, "var", "let", 0}, 1,
                                  "--- a/a.fj\n+++ b/a.fj\n@@ -1,1 +1,1 @@\n-var\n\\ No newline at end of file\n+let\n\\ No newline at end of file\n"));
    EDICOES_VERIFICA(c, DiffIgual("var\n", NULL, 0, ""));
}

static void TestaLinhaNova(const Contagem *c)
{
    const Edicao quebra[] = {{2, 1, 2, "b", "x\ny\nz", 0}, {4, 1, 1, "", "w", 0}};

    EDICOES_VERIFICA(c, EdicoesLinhaNova(quebra, 2, 1) == 1);
    EDICOES_VERIFICA(c, EdicoesLinhaNova(quebra, 2, 2) == 2);
    EDICOES_VERIFICA(c, EdicoesLinhaNova(quebra, 2, 3) == 5);
    EDICOES_VERIFICA(c, EdicoesLinhaNova(quebra, 2, 9) == 11);
    EDICOES_VERIFICA(c, EdicoesConflitam(&quebra[0], &quebra[0]) && !EdicoesConflitam(&quebra[0], &quebra[1]));
    EDICOES_VERIFICA(c, EdicoesIguais(&quebra[0], &quebra[0]) && !EdicoesIguais(&quebra[0], &quebra[1]));
}

static JsonValor *Json(Arena *a, const char *texto)
{
    return JsonLer(a, texto, strlen(texto));
}

static void TestaJson(const Contagem *c)
{
    Arena a = {0};
    const JsonValor *o = Json(&a, " {\"codigo\": \"E0707\", \"linha\": 7, \"neg\": -3, \"ok\": true, \"nada\": null, \"lista\": [1, \"dois\", {\"x\": 1.5e2}]} ");
    const JsonValor *lista = JsonMembro(o, "lista");

    EDICOES_VERIFICA(c, o != NULL && strcmp(JsonTextoDe(o, "codigo"), "E0707") == 0);
    EDICOES_VERIFICA(c, JsonNumeroDe(o, "linha", 0) == 7 && JsonNumeroDe(o, "neg", 0) == -3 && JsonNumeroDe(o, "ausente", 42) == 42);
    EDICOES_VERIFICA(c, JsonMembro(o, "ok")->tipo == JSON_BOOL && JsonMembro(o, "ok")->numero == 1 && JsonMembro(o, "nada")->tipo == JSON_NULO);
    EDICOES_VERIFICA(c, lista != NULL && lista->tipo == JSON_LISTA && lista->filho->numero == 1 && strcmp(lista->filho->prox->texto, "dois") == 0);
    EDICOES_VERIFICA(c, JsonNumeroDe(lista->filho->prox->prox, "x", 0) == 1);
    EDICOES_VERIFICA(c, strcmp(JsonTextoDe(Json(&a, "{\"t\": \"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\"}"), "t"), "a\"b\\c\n\xC3\xA9\xF0\x9F\x98\x80") == 0);
    EDICOES_VERIFICA(c, Json(&a, "{\"a\": 1") == NULL && Json(&a, "{\"a\": 1} x") == NULL && Json(&a, "[1,]") == NULL && Json(&a, "\"\\ud83d\"") == NULL);
    EDICOES_VERIFICA(c, Json(&a, "") == NULL && Json(&a, "{\"a\" 1}") == NULL && Json(&a, "tru") == NULL && Json(&a, "\"a\nb\"") == NULL);
    ArenaLiberar(&a);
}

/* IA1-I4: edicao que termina em outra linha (linha_fim), o que o trocar_no/trocar_corpo do 'forja editar' usa. */
static void TestaMultilinha(const Contagem *c)
{
    const Edicao junta = {1, 2, 2, "b\nc", "X", 2};
    const Edicao desce = {1, 2, 3, "b", "b\nc", 0};
    const Edicao errada = {1, 2, 2, "b\nz", "X", 2};
    const Edicao crlf = {1, 2, 2, "b\r\nc", "X", 2};
    Buf b = {0};

    EDICOES_VERIFICA(c, Resultado("ab\ncd\nef\n", &junta, 1, "aXd\nef\n"));
    EDICOES_VERIFICA(c, Resultado("ab\ncd\nef\n", &desce, 1, "ab\nc\ncd\nef\n"));
    EDICOES_VERIFICA(c, Resultado("ab\r\ncd\r\nef\r\n", &crlf, 1, "aXd\r\nef\r\n"));
    EDICOES_VERIFICA(c, Aplicar("ab\ncd\nef\n", &errada, 1, &b, NULL) == EDICAO_ANTIGO_DIFERE);
    BufLiberar(&b);
}

void EdicoesVerificacoes(int *total, int *falhas)
{
    const Contagem c = {total, falhas};

    TestaMultilinha(&c);
    TestaAplicar(&c);
    TestaRecusas(&c);
    TestaDiff(&c);
    TestaLinhaNova(&c);
    TestaJson(&c);
}
