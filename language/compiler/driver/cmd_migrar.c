/* cmd_migrar.c - 'forja migrar <arquivo.fj | pasta> ... [--dry-run | --aplicar] [--comentarios] [--json]' (ver cmd_migrar.h).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - IA1-L3f: liga o modo "tolerar forma antiga" (RenomeadoTolerarAntiga) durante o migrar: a forma antiga segue legivel com recusar = true, sem E0110.
 *            0.2.0 - IA1-L3d2: --comentarios (MigrarEntrada.comentarios; trocas de comentario com origem "comentario" e a lista comentarios_revisao no JSON). --prova segue rejeitada (exit 2).
 *            0.1.0 - IA1-L3d1: primeira versao. A analise de cada arquivo e do migrar_analise.c; aqui ficam os argumentos, a lista de arquivos (migrar_pasta.c), a gravacao atomica e o
 *            documento JSON. As edicoes passam pelo motor edicoes.c, o mesmo do 'forja corrigir'.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache/entradas_ler.h"
#include "common/fonte.h"
#include "common/renomeados.h"
#include "driver/cmd_migrar.h"
#include "driver/edicoes.h"
#include "driver/migrar_analise.h"
#include "driver/migrar_pasta.h"
#include "nativo/nativo_json.h"

enum { SAIDA_OK = 0, SAIDA_NAO_ANALISADO = 1, SAIDA_USO = 2 };

typedef struct {
    const char **entradas;
    int nentradas;
    bool aplicar, dry_run, comentarios;
} Pedido;

/* O que o documento resume: trocas, arquivos gravados, arquivos que nao puderam ser analisados, comentarios e se todas as linhas ficaram iguais. */
typedef struct {
    uint32_t trocas, alterados, nao_analisados, comentarios_reescritos, comentarios_revisao;
    bool linhas_iguais;
} Totais;

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: migrar: %s\n", motivo);
    fputs("uso: forja migrar <arquivo.fj | pasta> ... [--dry-run | --aplicar] [--comentarios] [--json]\n", stderr);
    return SAIDA_USO;
}

static const char *LerArgumentos(int argc, char **argv, Pedido *p)
{
    memset(p, 0, sizeof *p);
    p->entradas = calloc((size_t)argc + 1, sizeof *p->entradas);
    if (p->entradas == NULL) FalhaInterna("memoria esgotada lendo os argumentos do migrar");
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--json") == 0) continue;
        if (strcmp(argv[k], "--aplicar") == 0) p->aplicar = true;
        else if (strcmp(argv[k], "--dry-run") == 0) p->dry_run = true;
        else if (strcmp(argv[k], "--comentarios") == 0) p->comentarios = true;
        else if (argv[k][0] == '-') return "opcao desconhecida (valem --dry-run, --aplicar, --comentarios e --json)";
        else p->entradas[p->nentradas++] = argv[k];
    }
    if (p->aplicar && p->dry_run) return "--dry-run e --aplicar nao valem juntos";
    return p->nentradas == 0 ? "falta o arquivo .fj ou a pasta" : NULL;
}

/* ---------- saida JSON ---------- */

static void EmitirDetalhe(Buf *b, const MigrarTroca *t)
{
    BufFormatar(b, "{\"linha\": %u, \"coluna\": %u, \"coluna_fim\": %u, \"classe\": \"%s\", \"antigo\": ", t->linha, t->coluna, t->coluna_fim, RenomeadoClasseTexto(t->classe));
    EdicoesJsonTexto(b, t->antigo, strlen(t->antigo));
    BufAnexar(b, ", \"novo\": ");
    EdicoesJsonTexto(b, t->novo, strlen(t->novo));
    if (t->comentario) BufAnexar(b, ", \"origem\": \"comentario\"");
    BufAnexar(b, "}");
}

static void EmitirRevisao(Buf *b, const MigrarRevisao *r)
{
    BufFormatar(b, "{\"estado\": \"revisao_comentario\", \"linha\": %u, \"coluna\": %u, \"trecho\": ", r->linha, r->coluna);
    EdicoesJsonTexto(b, r->trecho, strlen(r->trecho));
    BufAnexar(b, ", \"formas_antigas_encontradas\": [");
    for (uint32_t k = 0; k < r->nformas; k++) {
        if (k > 0) BufAnexar(b, ", ");
        EdicoesJsonTexto(b, r->formas[k], strlen(r->formas[k]));
    }
    BufAnexar(b, "]}");
}

static void EmitirRevisoes(Buf *b, const MigrarResultado *r)
{
    BufAnexar(b, ", \"comentarios_revisao\": [");
    for (uint32_t k = 0; k < r->nrevisoes; k++) {
        if (k > 0) BufAnexar(b, ", ");
        EmitirRevisao(b, &r->revisoes[k]);
    }
    BufAnexar(b, "]");
}

static void EmitirArquivo(Buf *b, const char *caminho, const MigrarResultado *r, bool comentarios)
{
    BufAnexar(b, "  {\"arquivo\": ");
    JsonCaminho(b, caminho);
    BufFormatar(b, ", \"trocas\": %u, \"detalhes\": [", r->ntrocas);
    for (uint32_t k = 0; k < r->ntrocas; k++) {
        if (k > 0) BufAnexar(b, ", ");
        EmitirDetalhe(b, &r->trocas[k]);
    }
    BufFormatar(b, "], \"estado\": \"%s\", \"diagnostico\": ", MigrarEstadoTexto(r->estado));
    EdicoesJsonTextoOuNulo(b, r->diagnostico);
    BufAnexar(b, ", \"motivo\": ");
    EdicoesJsonTextoOuNulo(b, r->motivo);
    if (comentarios) EmitirRevisoes(b, r);
    BufAnexar(b, "}");
}

/* O fecho do documento: totais e, so com --comentarios, os de comentario. */
static void EmitirRodape(Buf *b, const Totais *t, bool comentarios)
{
    BufFormatar(b, "\n], \"total_trocas\": %u, \"arquivos_alterados\": %u, \"linhas_iguais\": %s", t->trocas, t->alterados, t->linhas_iguais ? "true" : "false");
    if (comentarios) BufFormatar(b, ", \"comentarios_reescritos\": %u, \"comentarios_revisao\": %u", t->comentarios_reescritos, t->comentarios_revisao);
    BufAnexar(b, "}\n");
}

/* ---------- um arquivo ---------- */

static bool GravarMigrado(const char *caminho, const MigrarResultado *r)
{
    int erro = 0;

    if (ArquivoGravarAtomico(caminho, r->novo.dados, r->novo.tam, &erro)) return true;
    fprintf(stderr, "forja: migrar: nao consegui gravar '%s' (errno %d)\n", caminho, erro);
    return false;
}

static void SomarTotais(Totais *t, const MigrarResultado *r, bool gravou)
{
    t->trocas += r->ntrocas;
    t->alterados += gravou ? 1 : 0;
    t->nao_analisados += r->estado == MIGRAR_NAO_ANALISADO ? 1 : 0;
    t->linhas_iguais = t->linhas_iguais && r->linhas_iguais;
    t->comentarios_revisao += r->nrevisoes;
    for (uint32_t k = 0; k < r->ntrocas; k++) t->comentarios_reescritos += r->trocas[k].comentario ? 1 : 0;
}

/* Le, analisa, grava (com --aplicar) e acrescenta o arquivo ao documento. false = erro de leitura ou gravacao (mensagem no stderr). */
static bool ProcessarArquivo(const char *caminho, const Pedido *p, Buf *json, Totais *tot)
{
    char *original = NULL;
    size_t tam = 0;
    Arena arena = {0};
    MigrarResultado r;
    bool gravou = false, ok = true;

    if (!ArquivoLerInteiro(caminho, &original, &tam)) {
        fprintf(stderr, "forja: migrar: nao consegui ler '%s'\n", caminho);
        return false;
    }
    MigrarArquivo(&(MigrarEntrada){caminho, original, tam, p->comentarios}, &arena, &r);
    if (p->aplicar && r.ntrocas > 0) {
        ok = GravarMigrado(caminho, &r);
        gravou = ok;
    }
    if (ok) {
        if (json->tam > 0) BufAnexar(json, ",\n");
        EmitirArquivo(json, caminho, &r, p->comentarios);
        SomarTotais(tot, &r, gravou);
    }
    BufLiberar(&r.novo);
    ArenaLiberar(&arena);
    free(original);
    return ok;
}

/* ---------- comando ---------- */

/* Junta os arquivos de todas as entradas; false = alguma entrada nao existe (mensagem no stderr). */
static bool ListarArquivos(const Pedido *p, Arena *arena, Vec *arquivos)
{
    for (int k = 0; k < p->nentradas; k++) {
        if (MigrarColetar(p->entradas[k], arena, arquivos)) continue;
        fprintf(stderr, "forja: migrar: nao achei '%s'\n", p->entradas[k]);
        return false;
    }
    return true;
}

static int Executar(const Pedido *p)
{
    Arena arena = {0};
    Vec arquivos;
    Buf json = {0}, doc = {0};
    Totais tot = {.linhas_iguais = true};
    int codigo = SAIDA_OK;

    VecIniciar(&arquivos, sizeof(const char *));
    if (!ListarArquivos(p, &arena, &arquivos)) codigo = SAIDA_USO;
    for (size_t k = 0; codigo == SAIDA_OK && k < arquivos.n; k++)
        if (!ProcessarArquivo(*(const char **)VecEm(&arquivos, k), p, &json, &tot)) codigo = SAIDA_USO;
    if (codigo == SAIDA_OK) {
        BufAnexar(&doc, "{\"arquivos\": [\n");
        if (json.dados != NULL) BufAnexarN(&doc, json.dados, json.tam);
        EmitirRodape(&doc, &tot, p->comentarios);
        fwrite(doc.dados, 1, doc.tam, stdout);
        codigo = tot.nao_analisados > 0 ? SAIDA_NAO_ANALISADO : SAIDA_OK;
    }
    BufLiberar(&json);
    BufLiberar(&doc);
    VecLiberar(&arquivos);
    ArenaLiberar(&arena);
    return codigo;
}

int RodarMigrar(int argc, char **argv)
{
    Pedido pedido;
    const char *erro = LerArgumentos(argc, argv, &pedido);
    int codigo;

    if (erro != NULL) {
        free(pedido.entradas);
        return UsoErrado(erro);
    }
    RenomeadoTolerarAntiga(true);
    codigo = Executar(&pedido);
    RenomeadoTolerarAntiga(false);
    free(pedido.entradas);
    return codigo;
}
