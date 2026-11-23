/* corrigir_diag.c - roda "forja check" num filho e le os diagnosticos com as correcoes aplicaveis (ver corrigir_diag.h).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - 0.21.0 F4 passo 2b: include driver/processo.h (cc.h saiu); ProcArquivoTemporario no lugar do CcArquivoTemporario.
 *            0.2.0 - IA1-I4: DiagLido.bruto (copia da linha JSON na arena) e CorrigirTemCodigo.
 *            0.1.0 - IA1-b05: primeira versao.
 * Verificacao: construir.bat && python tests\ia_first\verificar_corrigir.py --forja <forja.exe>
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/entradas_ler.h"
#include "driver/corrigir_diag.h"
#include "driver/json_leitor.h"
#include "driver/processo.h"
#include "driver/processo_win.h"

enum { CAMINHO_MAX = 600, ERRO_FILHO_MAX = 300, MAX_ERROS_DO_CHECK = 100 };

bool CorrigirMesmoCaminho(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        int ca = *a == '\\' ? '/' : tolower((unsigned char)*a), cb = *b == '\\' ? '/' : tolower((unsigned char)*b);

        if (ca != cb) return false;
    }
    return *a == *b;
}

bool CorrigirTemCodigo(const ResultadoCheck *r, const char *codigo)
{
    for (uint32_t k = 0; k < r->n; k++)
        if (strcmp(r->itens[k].codigo, codigo) == 0) return true;
    return false;
}

/* ---------- JSON -> DiagLido ---------- */

static bool LerEdicao(const JsonValor *j, const char *alvo, Edicao *e)
{
    const char *arquivo = JsonTextoDe(j, "arquivo"), *antigo = JsonTextoDe(j, "antigo"), *novo = JsonTextoDe(j, "novo");

    e->linha = (uint32_t)JsonNumeroDe(j, "linha", 0);
    e->coluna = (uint32_t)JsonNumeroDe(j, "coluna", 0);
    e->coluna_fim = (uint32_t)JsonNumeroDe(j, "coluna_fim", 0);
    e->linha_fim = (uint32_t)JsonNumeroDe(j, "linha_fim", 0);
    e->antigo = antigo != NULL ? antigo : "";
    e->novo = novo != NULL ? novo : "";
    return arquivo != NULL && CorrigirMesmoCaminho(arquivo, alvo);
}

static void LerDiagnostico(Arena *arena, const JsonValor *obj, const char *alvo, DiagLido *d)
{
    const JsonValor *lista = JsonMembro(obj, "correcao");
    uint32_t n = 0;

    d->codigo = JsonTextoDe(obj, "codigo");
    d->arquivo = JsonTextoDe(obj, "arquivo");
    d->confianca = JsonTextoDe(obj, "confianca");
    d->sugestao = JsonTextoDe(obj, "sugestao");
    d->id = JsonTextoDe(obj, "id");
    d->linha = (uint32_t)JsonNumeroDe(obj, "linha", 0);
    d->coluna = (uint32_t)JsonNumeroDe(obj, "coluna", 0);
    d->edicoes_no_alvo = true;
    if (lista == NULL || lista->tipo != JSON_LISTA) return;
    for (const JsonValor *j = lista->filho; j != NULL; j = j->prox) n++;
    d->edicoes = ArenaAlocar(arena, (n > 0 ? n : 1) * sizeof *d->edicoes);
    for (const JsonValor *j = lista->filho; j != NULL; j = j->prox)
        if (!LerEdicao(j, alvo, &d->edicoes[d->nedicoes++])) d->edicoes_no_alvo = false;
}

/* Cada linha do stdout e um diagnostico em JSON; o que nao for objeto com "codigo" e ignorado.
 * LIMITE: parametros 5 (texto+tam e um buffer; arena, alvo e resultado sao os tres donos da leitura; agrupar em struct so criaria um tipo de uso unico). */
static void LerDiagnosticos(Arena *arena, const char *alvo, const char *texto, size_t tam, ResultadoCheck *r)
{
    uint32_t linhas = 0;
    size_t ini = 0;

    for (size_t k = 0; k < tam; k++) linhas += texto[k] == '\n';
    r->itens = ArenaAlocar(arena, ((size_t)linhas + 1) * sizeof *r->itens);
    while (ini < tam) {
        const char *nl = memchr(texto + ini, '\n', tam - ini);
        size_t fim = nl != NULL ? (size_t)(nl - texto) : tam;
        const JsonValor *obj = fim > ini ? JsonLer(arena, texto + ini, fim - ini) : NULL;

        if (obj != NULL && JsonTextoDe(obj, "codigo") != NULL) {
            LerDiagnostico(arena, obj, alvo, &r->itens[r->n]);
            r->itens[r->n++].bruto = ArenaCopiarTexto(arena, texto + ini, fim - ini);
        }
        ini = fim + 1;
    }
}

/* ---------- filho ---------- */

/* Dispara "forja check" com stdout e stderr em arquivos; devolve o exit do filho em *saida.
 * LIMITE: parametros 6 (alvo, os dois arquivos de saida, o exit e o par buffer+tamanho do erro; funcao de uso unico, struct nao ganha clareza). */
static bool DispararCheck(const char *alvo, const char *arq_out, const char *arq_err, int *saida, char *erro, size_t tam_erro)
{
    char exe[CAMINHO_MAX];
    const char *args[] = {"check", "--max-erros", "100", alvo, NULL};
    ProcessoPedido pedido = {.exe = exe, .args = args, .saida_out = arq_out, .saida_err = arq_err};
    Processo filho;
    uint32_t qual;
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);

    if (n == 0 || n >= sizeof exe) {
        snprintf(erro, tam_erro, "nao descobri o caminho do proprio forja.exe");
        return false;
    }
    if (!ProcessoDisparar(&pedido, &filho)) {
        snprintf(erro, tam_erro, "nao consegui iniciar o check (erro do sistema %lu)", (unsigned long)filho.erro);
        return false;
    }
    if (!ProcessoEsperarQualquer(&filho, 1, &qual)) {
        snprintf(erro, tam_erro, "a espera pelo check falhou (erro do sistema %lu)", (unsigned long)GetLastError());
        return false;
    }
    *saida = ProcessoConcluir(&filho);
    return true;
}

/* A frase de erro quando o filho saiu com uso (2) ou erro interno (3): o 1o trecho do que ele escreveu no stderr. */
static void ErroDoFilho(const char *arq_err, int saida, char *erro, size_t tam_erro)
{
    char *dados = NULL;
    size_t tam = 0;
    int n = snprintf(erro, tam_erro, "o check terminou com exit %d", saida);

    if (!ArquivoLerInteiro(arq_err, &dados, &tam) || tam == 0 || n < 0 || (size_t)n >= tam_erro) {
        free(dados);
        return;
    }
    while (tam > 0 && (dados[tam - 1] == '\n' || dados[tam - 1] == '\r')) tam--;
    snprintf(erro + n, tam_erro - (size_t)n, ": %.*s", (int)(tam < ERRO_FILHO_MAX ? tam : ERRO_FILHO_MAX), dados);
    free(dados);
}

/* LIMITE: parametros 5 (o par buffer+tamanho do erro e a convencao do driver; o resto e arena, alvo e resultado). */
bool CorrigirRodarCheck(Arena *arena, const char *alvo, ResultadoCheck *resultado, char *erro, size_t tam_erro)
{
    char arq_out[CAMINHO_MAX], arq_err[CAMINHO_MAX], *dados = NULL;
    size_t tam = 0;
    bool ok = false;

    memset(resultado, 0, sizeof *resultado);
    if (!ProcArquivoTemporario(arq_out, sizeof arq_out, ".corrigir.out") || !ProcArquivoTemporario(arq_err, sizeof arq_err, ".corrigir.err")) {
        snprintf(erro, tam_erro, "o caminho da pasta temporaria e longo demais");
        return false;
    }
    if (DispararCheck(alvo, arq_out, arq_err, &resultado->saida, erro, tam_erro)) {
        if (resultado->saida > 1) {
            ErroDoFilho(arq_err, resultado->saida, erro, tam_erro);
        } else if (!ArquivoLerInteiro(arq_out, &dados, &tam)) {
            snprintf(erro, tam_erro, "nao consegui ler a saida do check");
        } else {
            LerDiagnosticos(arena, alvo, dados, tam, resultado);
            ok = true;
        }
    }
    free(dados);
    remove(arq_out);
    remove(arq_err);
    return ok;
}
