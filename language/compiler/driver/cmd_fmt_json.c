/* cmd_fmt_json.c - o documento JSON do 'forja fmt --json' (ver cmd_fmt_interno.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1b: primeira versao (objeto de arquivo, edicoes com a regra, primeiro desvio e o documento com totais).
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <stdio.h>
#include <string.h>

#include "driver/cmd_fmt_interno.h"
#include "driver/edicoes.h"
#include "nativo/nativo_json.h"

static const char *EstadoTexto(FmtEstadoArquivo estado)
{
    switch (estado) {
    case FMT_ARQ_CANONICO: return "canonico";
    case FMT_ARQ_MUDARIA: return "mudaria";
    case FMT_ARQ_GRAVADO: return "gravado";
    case FMT_ARQ_NAO_ANALISADO: return "nao_analisado";
    }
    return "nao_analisado";
}

/* Uma edicao: as posicoes, o antigo, o novo e a regra (linha_fim so quando a faixa tem varias linhas). */
static void EmitirEdicao(Buf *b, const Edicao *e, const char *regra)
{
    BufFormatar(b, "{\"linha\": %u, \"coluna\": %u, ", e->linha, e->coluna);
    if (e->linha_fim > e->linha) BufFormatar(b, "\"linha_fim\": %u, ", e->linha_fim);
    BufFormatar(b, "\"coluna_fim\": %u, \"antigo\": ", e->coluna_fim);
    EdicoesJsonTexto(b, e->antigo, strlen(e->antigo));
    BufAnexar(b, ", \"novo\": ");
    EdicoesJsonTexto(b, e->novo, strlen(e->novo));
    BufFormatar(b, ", \"regra\": \"%s\"}", regra);
}

static void EmitirEdicoes(Buf *b, const FmtResultado *r)
{
    const Edicao *lista = r->edicoes.dados;
    const char *const *regras = r->regras.dados;

    BufAnexar(b, "[");
    for (size_t k = 0; k < r->edicoes.n; k++) {
        if (k > 0) BufAnexar(b, ", ");
        EmitirEdicao(b, &lista[k], regras[k]);
    }
    BufAnexar(b, "]");
}

/* O primeiro desvio do arquivo, ou null quando ele ja e canonico. */
static void EmitirPrimeiro(Buf *b, const FmtResultado *r)
{
    if (r->desvios == 0) {
        BufAnexar(b, "null");
        return;
    }
    BufFormatar(b, "{\"linha\": %u, \"coluna\": %u, \"regra\": \"%s\"}", r->primeiro.linha, r->primeiro.coluna, r->primeiro.regra != NULL ? r->primeiro.regra : "-");
}

void FmtJsonArquivo(Buf *json, const char *caminho, FmtEstadoArquivo estado, const FmtResultado *r)
{
    if (json->tam > 0) BufAnexar(json, ",\n");
    BufAnexar(json, "  {\"arquivo\": ");
    JsonCaminho(json, caminho);
    BufFormatar(json, ", \"estado\": \"%s\", \"desvios\": %u, \"primeiro\": ", EstadoTexto(estado), r->desvios);
    EmitirPrimeiro(json, r);
    BufAnexar(json, ", \"edicoes\": ");
    EmitirEdicoes(json, r);
    BufAnexar(json, ", \"diagnostico\": ");
    EdicoesJsonTextoOuNulo(json, r->codigo);
    BufAnexar(json, ", \"motivo\": ");
    EdicoesJsonTextoOuNulo(json, r->motivo);
    if (estado == FMT_ARQ_NAO_ANALISADO) BufFormatar(json, ", \"diag_linha\": %u, \"diag_coluna\": %u", r->diag_linha, r->diag_coluna);
    BufAnexar(json, "}");
}

void FmtJsonDocumento(Buf *doc, const FmtExecucao *x)
{
    const FmtTotais *t = &x->totais;

    BufAnexar(doc, "{\"arquivos\": [\n");
    if (x->json.tam > 0) BufAnexarN(doc, x->json.dados, x->json.tam);
    BufFormatar(doc, "\n], \"totais\": {\"arquivos\": %u, \"canonicos\": %u, \"mudariam\": %u, \"gravados\": %u, \"nao_analisados\": %u}}\n",
                t->arquivos, t->canonicos, t->mudariam, t->gravados, t->nao_analisados);
}
