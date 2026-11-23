/* cmd_spec.c - 'forja spec [--cartao] [--secao S] [--json] [--exemplo CODIGO] [CODIGO]' (ver cmd_spec.h). O texto vem da doc embutida (common\doc_embutida.c), nunca do disco.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P79: --receitas imprime a colinha de bolso (receitas.txt embutido, inteiro; sem --secao).
 *            0.1.0 - IA1-b09: primeira versao. Sem opcao: a spec.txt como esta no arquivo (stdout em modo binario: o CRLF do arquivo sai sem virar CR CR LF); --cartao: o cartao.txt byte a byte;
 *            --secao S: uma secao (migracao = a migracao.txt); --json: {versao, secoes: [{id, titulo, texto}]}; CODIGO: o spec do codigo (o mesmo campo "spec" do diagnostico);
 *            --exemplo CODIGO: o par errado/certo da bateria (exemplos.txt).
 * Verificacao: construir.bat && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

#include "common/diag.h"
#include "common/diag_spec.h"
#include "common/spec_doc.h"
#include "common/spec_exemplos.h"
#include "driver/cmd_spec.h"

enum { SAIDA_OK = 0, SAIDA_USO = 2 };

typedef struct {
    bool cartao, json, receitas;
    const char *secao, *exemplo, *codigo;
} Pedido;

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: spec: %s\n", motivo);
    fputs("uso: forja spec [--cartao] [--receitas] [--secao S] [--json] [--exemplo CODIGO] [CODIGO]\n", stderr);
    return SAIDA_USO;
}

/* Opcao com valor: *k aponta a opcao e passa para o valor. */
static const char *LerValor(int argc, char **argv, int *k, const char **destino)
{
    if (*k + 1 >= argc) return "a opcao exige um valor";
    *destino = argv[++*k];
    return NULL;
}

/* Le as opcoes e o CODIGO posicional; devolve o motivo do uso errado ou NULL. */
static const char *LerOpcoes(int argc, char **argv, Pedido *p)
{
    const char *motivo = NULL;

    memset(p, 0, sizeof *p);
    for (int k = 0; k < argc && motivo == NULL; k++) {
        if (strcmp(argv[k], "--cartao") == 0) p->cartao = true;
        else if (strcmp(argv[k], "--receitas") == 0) p->receitas = true;
        else if (strcmp(argv[k], "--json") == 0) p->json = true;
        else if (strcmp(argv[k], "--secao") == 0) motivo = LerValor(argc, argv, &k, &p->secao);
        else if (strcmp(argv[k], "--exemplo") == 0) motivo = LerValor(argc, argv, &k, &p->exemplo);
        else if (argv[k][0] == '-') motivo = "opcao desconhecida";
        else if (p->codigo != NULL) motivo = "so um codigo por vez";
        else p->codigo = argv[k];
    }
    return motivo;
}

/* O que nao combina e o codigo que nao existe; devolve o motivo do uso errado ou NULL. */
static const char *ValidarPedido(const Pedido *p)
{
    if (p->receitas && (p->secao != NULL || p->cartao || p->json || p->exemplo != NULL || p->codigo != NULL)) return "--receitas nao combina com as outras opcoes";
    if (p->exemplo != NULL && (p->secao != NULL || p->cartao || p->codigo != NULL)) return "--exemplo nao combina com --secao, --cartao nem CODIGO";
    if (p->codigo != NULL && (p->secao != NULL || p->cartao)) return "CODIGO nao combina com --secao nem --cartao";
    if (p->codigo != NULL && DiagCodigoPorTexto(p->codigo) < 0) return "codigo de diagnostico desconhecido (veja 'forja explicar --lista')";
    if (p->exemplo != NULL && DiagCodigoPorTexto(p->exemplo) < 0) return "codigo de diagnostico desconhecido (veja 'forja explicar --lista')";
    return NULL;
}

static void EmitirBruto(DocTexto t)
{
    fwrite(t.texto, 1, t.tam, stdout);
}

/* ---------- secoes ---------- */

/* As secoes da fonte e, na spec, a migracao como a ultima ("migracao" = o migracao.txt inteiro). Devolve quantas. */
static size_t ListarSecoes(bool cartao, SpecSecao *lista)
{
    size_t n = SpecSecoes(cartao ? DOC_CARTAO : DOC_SPEC, lista, SPEC_SECOES_MAX);

    if (cartao) return n;
    snprintf(lista[n].nome, sizeof lista[n].nome, "migracao");
    lista[n].bloco = lista[n].corpo = DocEmbutida(DOC_MIGRACAO);
    return n + 1;
}

static int SecaoDesconhecida(const char *nome, const SpecSecao *lista, size_t n)
{
    fprintf(stderr, "forja: spec: secao desconhecida: %s\nsecoes:", nome);
    for (size_t k = 0; k < n; k++) fprintf(stderr, " %s", lista[k].nome);
    fputc('\n', stderr);
    return SAIDA_USO;
}

static int MostrarSecao(const Pedido *p)
{
    SpecSecao lista[SPEC_SECOES_MAX + 1];
    size_t n = ListarSecoes(p->cartao, lista);
    const SpecSecao *s = SpecAcharSecao(lista, n, p->secao);

    if (s == NULL) return SecaoDesconhecida(p->secao, lista, n);
    EmitirBruto(s->bloco);
    return SAIDA_OK;
}

static DocTexto SemFimEmBranco(DocTexto t)
{
    while (t.tam > 0 && strchr(" \r\n\t", t.texto[t.tam - 1]) != NULL) t.tam--;
    return t;
}

static void ImprimirSecaoJson(const SpecSecao *s)
{
    printf("{\"id\": \"%s\", \"titulo\": \"%c%s\", \"texto\": ", s->nome, toupper((unsigned char)s->nome[0]), s->nome + 1);
    SpecImprimirTextoJson(stdout, SemFimEmBranco(s->corpo));
    fputc('}', stdout);
}

static int MostrarJson(const Pedido *p, const char *versao)
{
    SpecSecao lista[SPEC_SECOES_MAX + 1];
    size_t n = ListarSecoes(p->cartao, lista);
    bool primeira = true;

    if (p->secao != NULL && SpecAcharSecao(lista, n, p->secao) == NULL) return SecaoDesconhecida(p->secao, lista, n);
    printf("{\"versao\": \"%s\", \"secoes\": [", versao);
    for (size_t k = 0; k < n; k++) {
        if (p->secao != NULL && strcmp(lista[k].nome, p->secao) != 0) continue;
        if (!primeira) fputs(", ", stdout);
        ImprimirSecaoJson(&lista[k]);
        primeira = false;
    }
    fputs("]}\n", stdout);
    return SAIDA_OK;
}

/* ---------- codigo e exemplo ---------- */

static int MostrarSpecDoCodigo(const Pedido *p)
{
    char texto[DIAG_SPEC_MAX];

    DiagSpecTexto(p->codigo, texto, sizeof texto);
    if (!p->json) {
        printf("%s\n", texto);
        return SAIDA_OK;
    }
    printf("{\"codigo\": \"%s\", \"spec\": ", p->codigo);
    SpecImprimirTextoJson(stdout, (DocTexto){texto, strlen(texto)});
    fputs("}\n", stdout);
    return SAIDA_OK;
}

static void ImprimirBlocoTexto(DocTexto bloco)
{
    DocTexto linha;

    while (SpecProximaLinha(&bloco, &linha)) {
        size_t pular = linha.tam >= 2 ? 2 : linha.tam;

        fputs("    ", stdout);
        fwrite(linha.texto + pular, 1, linha.tam - pular, stdout);
        fputc('\n', stdout);
    }
}

static void ImprimirExemploTexto(const SpecExemplo *e)
{
    printf("exemplo %s (%.*s, linha %u)\nerrado:\n", e->codigo, (int)e->fixture.tam, e->fixture.texto, e->linha);
    ImprimirBlocoTexto(e->errado);
    if (e->certo.tam > 0) {
        fputs("certo:\n", stdout);
        ImprimirBlocoTexto(e->certo);
    } else {
        fputs("certo: (a bateria nao tem conserto automatico verificado para este codigo)\n", stdout);
    }
    printf("forma certa: %.*s\n", (int)e->forma.tam, e->forma.texto);
}

static int MostrarExemplo(const Pedido *p)
{
    SpecExemplo e;

    if (!SpecExemploDoCodigo(p->exemplo, &e)) {
        fprintf(stderr, "forja: spec: %s nao tem exemplo na bateria (veja 'forja explicar %s')\n", p->exemplo, p->exemplo);
        return SAIDA_USO;
    }
    if (p->json) {
        SpecImprimirExemploJson(stdout, &e);
        fputc('\n', stdout);
    } else {
        ImprimirExemploTexto(&e);
    }
    return SAIDA_OK;
}

int RodarSpec(int argc, char **argv, const char *versao)
{
    Pedido p;
    const char *motivo = LerOpcoes(argc, argv, &p);

    if (motivo == NULL) motivo = ValidarPedido(&p);
    if (motivo != NULL) return UsoErrado(motivo);
    _setmode(_fileno(stdout), _O_BINARY); /* o texto da doc ja leva o CRLF dele: o modo texto o dobraria */
    if (p.receitas) {
        EmitirBruto(DocEmbutida(DOC_RECEITAS));
        return SAIDA_OK;
    }
    if (p.exemplo != NULL) return MostrarExemplo(&p);
    if (p.codigo != NULL) return MostrarSpecDoCodigo(&p);
    if (p.json) return MostrarJson(&p, versao);
    if (p.secao != NULL) return MostrarSecao(&p);
    EmitirBruto(DocEmbutida(p.cartao ? DOC_CARTAO : DOC_SPEC));
    return SAIDA_OK;
}
