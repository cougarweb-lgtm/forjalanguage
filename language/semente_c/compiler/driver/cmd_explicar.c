/* cmd_explicar.c - 'forja explicar --lista [--json]' e 'forja explicar [CODIGO] --exemplos [--json]' (ver cmd_spec.h). O 'forja explicar CODIGO' simples segue no main.c.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao. --lista: uma linha por codigo "CODIGO<TAB>etapa<TAB>spec<TAB>titulo" (com --json, um array [{codigo, titulo, etapa, spec}]);
 *            etapa pela letra do codigo (E check, R execucao, B build, G ligar, X backend), spec = a secao do mapa_spec.tsv, titulo = a 1a frase da explicacao do catalogo.
 *            --exemplos: uma linha JSON {codigo, exemplos: [...]} (sem CODIGO, uma por codigo do catalogo); os exemplos vem do exemplos.txt embutido (par errado/certo da bateria).
 * Verificacao: construir.bat && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#include <stdio.h>
#include <string.h>

#include "common/diag.h"
#include "common/spec_doc.h"
#include "common/spec_exemplos.h"
#include "driver/cmd_spec.h"

enum { SAIDA_OK = 0, SAIDA_USO = 2, TITULO_MAX = 100 };

typedef struct {
    bool lista, exemplos, json;
    const char *codigo;
} Pedido;

bool ExplicarEhExtenso(int argc, char **argv)
{
    for (int k = 0; k < argc; k++)
        if (strcmp(argv[k], "--lista") == 0 || strcmp(argv[k], "--exemplos") == 0) return true;
    return false;
}

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: explicar: %s\n", motivo);
    fputs("uso: forja explicar <CODIGO>\n     forja explicar --lista [--json]\n     forja explicar [CODIGO] --exemplos\n", stderr);
    return SAIDA_USO;
}

static const char *LerArgumentos(int argc, char **argv, Pedido *p)
{
    memset(p, 0, sizeof *p);
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--lista") == 0) p->lista = true;
        else if (strcmp(argv[k], "--exemplos") == 0) p->exemplos = true;
        else if (strcmp(argv[k], "--json") == 0) p->json = true;
        else if (argv[k][0] == '-') return "opcao desconhecida";
        else if (p->codigo != NULL) return "so um codigo por vez";
        else p->codigo = argv[k];
    }
    if (p->lista && (p->exemplos || p->codigo != NULL)) return "--lista nao combina com --exemplos nem com CODIGO";
    if (p->codigo != NULL && DiagCodigoPorTexto(p->codigo) < 0) return "codigo de diagnostico desconhecido (veja 'forja explicar --lista')";
    return NULL;
}

/* ---------- --lista ---------- */

static const char *EtapaDoCodigo(const char *codigo)
{
    switch (codigo[0]) {
    case 'E': return "check";
    case 'R': return "execucao";
    case 'B': return "build";
    case 'G': return "ligar";
    default: return "backend";
    }
}

/* A 1a frase da explicacao do catalogo (ate o ". " ou o fim), no maximo TITULO_MAX bytes cortados em palavra. */
static void TituloDoCodigo(DiagCodigo codigo, char *saida)
{
    const char *texto = DiagCodigoExplicacao(codigo);
    const char *ponto = strstr(texto, ". ");
    size_t n = ponto != NULL ? (size_t)(ponto - texto) : strlen(texto);

    if (n > 0 && ponto == NULL && texto[n - 1] == '.') n--;
    if (n > TITULO_MAX) {
        n = TITULO_MAX;
        while (n > 1 && texto[n] != ' ') n--;
    }
    memcpy(saida, texto, n);
    saida[n] = '\0';
}

typedef struct {
    const char *codigo, *etapa, *spec;
    char titulo[TITULO_MAX + 1];
} ItemLista;

/* Os dados de um codigo; o spec e a secao do mapa ("" se o mapa nao conhece o codigo). */
static void MontarItem(DiagCodigo codigo, ItemLista *item, char secao[SPEC_NOME_MAX])
{
    item->codigo = DiagCodigoTexto(codigo);
    item->etapa = EtapaDoCodigo(item->codigo);
    if (!SpecSecaoDoCodigo(item->codigo, secao)) secao[0] = '\0';
    item->spec = secao;
    TituloDoCodigo(codigo, item->titulo);
}

static void ImprimirItemJson(const ItemLista *item)
{
    printf("{\"codigo\": \"%s\", \"titulo\": ", item->codigo);
    SpecImprimirTextoJson(stdout, (DocTexto){item->titulo, strlen(item->titulo)});
    printf(", \"etapa\": \"%s\", \"spec\": \"%s\"}", item->etapa, item->spec);
}

static int ListarCodigos(bool json)
{
    if (json) fputc('[', stdout);
    for (int k = 0; k < DIAG_QTD; k++) {
        ItemLista item;
        char secao[SPEC_NOME_MAX];

        MontarItem((DiagCodigo)k, &item, secao);
        if (json) printf("%s", k > 0 ? ", " : "");
        if (json) ImprimirItemJson(&item);
        else printf("%s\t%s\t%s\t%s\n", item.codigo, item.etapa, item.spec, item.titulo);
    }
    if (json) fputs("]\n", stdout);
    return SAIDA_OK;
}

/* ---------- --exemplos ---------- */

static void ImprimirExemplosDoCodigo(const char *codigo)
{
    SpecExemplo e;

    printf("{\"codigo\": \"%s\", \"exemplos\": [", codigo);
    if (SpecExemploDoCodigo(codigo, &e)) SpecImprimirExemploJson(stdout, &e);
    fputs("]}\n", stdout);
}

static int ListarExemplos(const char *codigo)
{
    if (codigo != NULL) {
        ImprimirExemplosDoCodigo(codigo);
        return SAIDA_OK;
    }
    for (int k = 0; k < DIAG_QTD; k++) ImprimirExemplosDoCodigo(DiagCodigoTexto((DiagCodigo)k));
    return SAIDA_OK;
}

int RodarExplicarExtenso(int argc, char **argv)
{
    Pedido p;
    const char *motivo = LerArgumentos(argc, argv, &p);

    if (motivo != NULL) return UsoErrado(motivo);
    if (p.lista) return ListarCodigos(p.json);
    return ListarExemplos(p.codigo);
}
