/* cmd_mapa.c - 'forja mapa' (ver cmd_mapa.h): le os argumentos, compila o projeto (a compilacao e o indice do 'forja consultar') e escreve o mapa em JSON; o resumo em texto
 * fica em mapa_resumo.c.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b: os bytes do modulo somam os arquivos dele (ConsultaBytesDoModulo); "arquivo" em cada tipo so num pacote de 2+ arquivos.
 *            0.1.0 - IA1-I3: primeira versao. O --json traz tamanho, entrada, modulos, api (as mesmas linhas de 'consultar api'), tipos, testes e dependencias; o std embutido entra so em
 *            'modulos' e em 'dependencias' (e inteiro com --modulo std:x).
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/cmd_mapa.h"
#include "driver/mapa_interno.h"

#define MAPA_USO "uso: forja mapa [<pasta | arq.fj>] [--json | --resumo] [--modulo <caminho>] [--limite <N>]\n"

/* ---------- argumentos ---------- */

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: mapa: %s\n" MAPA_USO, motivo);
    return CONSULTAR_USO;
}

/* "--limite N": inteiro positivo; 0 = invalido. */
static uint32_t LerLimite(const char *texto)
{
    char *fim;
    unsigned long n = strtoul(texto, &fim, 10);

    return (texto[0] == '\0' || *fim != '\0' || n == 0 || n > 100000) ? 0 : (uint32_t)n;
}

/* Le uma opcao com valor (argv[*k] e a opcao): devolve o motivo do erro ou NULL. */
static const char *LerOpcaoComValor(int argc, char **argv, int *k, PedidoConsulta *p)
{
    if (*k + 1 >= argc) return "a opcao exige um valor";
    if (strcmp(argv[*k], "--modulo") == 0) {
        p->modulo = argv[++*k];
        return NULL;
    }
    p->limite = LerLimite(argv[++*k]);
    return p->limite == 0 ? "--limite exige um inteiro positivo" : NULL;
}

/* As opcoes que nao combinam; p->resumo vira o padrao quando nao veio --json. */
static const char *ConferirCombinacao(PedidoConsulta *p, bool json, bool resumo)
{
    if (json && resumo) return "--json e --resumo nao vao juntos";
    if (json && p->limite != 0) return "--limite so vale com --resumo";
    p->resumo = !json;
    return NULL;
}

/* json = --json escrito; p->resumo = --resumo escrito ou nenhum dos dois (o padrao). Devolve o motivo do uso errado ou NULL. */
static const char *LerArgumentos(int argc, char **argv, PedidoConsulta *p, bool *json)
{
    bool resumo = false;
    const char *motivo;

    memset(p, 0, sizeof *p);
    p->pergunta = "mapa";
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--json") == 0) *json = true;
        else if (strcmp(argv[k], "--resumo") == 0) resumo = true;
        else if (strcmp(argv[k], "--modulo") == 0 || strcmp(argv[k], "--limite") == 0) {
            motivo = LerOpcaoComValor(argc, argv, &k, p);
            if (motivo != NULL) return motivo;
        } else if (strncmp(argv[k], "--", 2) == 0) return "opcao desconhecida (valem --json, --resumo, --modulo e --limite)";
        else if (p->alvo == NULL) p->alvo = argv[k];
        else return "argumentos demais";
    }
    return ConferirCombinacao(p, *json, resumo);
}

/* ---------- tamanho e escopo ---------- */

bool MapaEhTipo(const Decl *d)
{
    return d->tipo_no == D_STRUCT || d->tipo_no == D_ENUM || d->tipo_no == D_UNIAO;
}

bool MapaNoEscopo(const Consulta *q, int modulo, int so)
{
    if (so >= 0) return modulo == so;
    return q->c->modulos.itens[modulo].origem != MOD_STD;
}

void MapaMedir(const Consulta *q, MapaTamanho *t)
{
    memset(t, 0, sizeof *t);
    for (uint32_t k = 0; k < q->c->modulos.n; k++) {
        if (q->c->modulos.itens[k].origem == MOD_STD) {
            t->externos++;
            continue;
        }
        t->modulos++;
        t->linhas += ConsultaLinhasDoModulo(q, (uint8_t)k);
        t->bytes += ConsultaBytesDoModulo(q, (uint8_t)k);
    }
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (q->c->modulos.itens[d->modulo].origem == MOD_STD) continue;
        t->simbolos += ConsultaEhSimbolo(d);
        t->publicos += ConsultaEhApi(d);
        t->tipos += MapaEhTipo(d);
        t->testes += d->tipo_no == D_TEST;
    }
}

static void TamanhoJson(Consulta *q)
{
    MapaTamanho t;

    MapaMedir(q, &t);
    BufAnexar(&q->saida, ", \"tamanho\": {");
    BufFormatar(&q->saida, "\"modulos\": %u, \"externos\": %u, \"linhas\": %llu, \"bytes\": %llu, ", t.modulos, t.externos, (unsigned long long)t.linhas, (unsigned long long)t.bytes);
    BufFormatar(&q->saida, "\"simbolos\": %u, \"publicos\": %u, \"tipos\": %u, \"testes\": %u}", t.simbolos, t.publicos, t.tipos, t.testes);
}

/* ---------- secoes do JSON ---------- */

static void EntradasJson(Consulta *q, int so)
{
    ConsultaAbrirLista(q, "entrada");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        const char *entrada = ConsultaEntradaDe(q, d);
        if (entrada != NULL && MapaNoEscopo(q, d->modulo, so)) ConsultaEntradaJson(q, d, entrada);
    }
    ConsultaFecharLista(q);
}

static void ModulosJson(Consulta *q)
{
    uint8_t ordem[MODULOS_MAX];
    int n = ConsultaOrdemDosModulos(q, ordem);

    ConsultaAbrirLista(q, "modulos");
    for (int i = 0; i < n; i++) ConsultaModuloJson(q, ordem[i]);
    ConsultaFecharLista(q);
}

static bool ModuloTemApi(const Consulta *q, uint8_t modulo)
{
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo == modulo && ConsultaEhApi(d)) return true;
    }
    return false;
}

static void ApiJson(Consulta *q, int so)
{
    uint8_t ordem[MODULOS_MAX];
    int n = ConsultaOrdemDosModulos(q, ordem);

    ConsultaAbrirLista(q, "api");
    for (int i = 0; i < n; i++) {
        if (!MapaNoEscopo(q, ordem[i], so) || !ModuloTemApi(q, ordem[i])) continue;
        ConsultaAbrirObj(q);
        ConsultaChaveModulo(q, "modulo", ordem[i]);
        ConsultaApiDoModuloJson(q, ordem[i]);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

static void CamposDoTipoJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirLista(q, d->tipo_no == D_UNIAO ? "variantes" : "campos");
    for (uint32_t i = 0; i < d->ncampos; i++) {
        ConsultaAbrirObj(q);
        if (d->tipo_no == D_STRUCT) ConsultaChaveNome(q, "nome", d->campos[i].nome);
        ConsultaChaveTipo(q, "tipo", d->campos[i].tipo.tipo);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

static void MembrosDoTipoJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirLista(q, "membros");
    for (uint32_t i = 0; i < d->nmembros; i++) {
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "nome", d->membros[i].nome);
        if (d->op == 1) ConsultaChaveInt(q, "valor", d->membros[i].valor);
        else ConsultaChaveTexto(q, "valor", NULL, 0);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

static void TipoJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "categoria", IndiceCategoria(d), strlen(IndiceCategoria(d)));
    ConsultaChaveId(q, "id", d);
    ConsultaChaveModulo(q, "modulo", d->modulo);
    ConsultaChaveBool(q, "publico", d->publico);
    ConsultaChaveArquivoSePacote(q, d->pos);
    ConsultaChavePos(q, "pos", ConsultaInicioDecl(q, d));
    if (d->tipo_no == D_ENUM) MembrosDoTipoJson(q, d);
    else CamposDoTipoJson(q, d);
    ConsultaFecharObj(q);
}

static void TiposJson(Consulta *q, int so)
{
    ConsultaAbrirLista(q, "tipos");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (MapaEhTipo(d) && MapaNoEscopo(q, d->modulo, so)) TipoJson(q, d);
    }
    ConsultaFecharLista(q);
}

static void TestesJson(Consulta *q, int so)
{
    ConsultaAbrirLista(q, "testes");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (ConsultaEhTesteDoProjeto(q, d) && MapaNoEscopo(q, d->modulo, so)) ConsultaTesteJson(q, d);
    }
    ConsultaFecharLista(q);
}

/* Modulos de fora da pasta do projeto (std, vendor, engine) que algum import traz, e os 'link' de foreign. */
static void DependenciasJson(Consulta *q)
{
    ConsultaAbrirObj(q);
    ConsultaAbrirLista(q, "externas");
    for (uint32_t k = 1; k < q->c->modulos.n; k++) {
        if (q->c->modulos.itens[k].origem != MOD_ARQUIVO) ConsultaItemModulo(q, (uint8_t)k);
    }
    ConsultaFecharLista(q);
    ConsultaLinksJson(q, "links", -1);
    ConsultaFecharObj(q);
}

static int MapaJson(Consulta *q, int so)
{
    ConsultaAbrir(q, "mapa");
    TamanhoJson(q);
    EntradasJson(q, so);
    ModulosJson(q);
    ApiJson(q, so);
    TiposJson(q, so);
    TestesJson(q, so);
    BufAnexar(&q->saida, ", \"dependencias\": ");
    q->primeiro = true;
    DependenciasJson(q);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}

static int MapaResponder(Consulta *q, const PedidoConsulta *p)
{
    int so = p->modulo != NULL ? ConsultaAcharModulo(q, p->modulo) : -1;

    if (p->modulo != NULL && so < 0) return ConsultaUsoErrado("mapa", p->modulo, "nao e modulo do projeto (use o caminho do import, ex.: std:mem)");
    _setmode(_fileno(stdout), _O_BINARY); /* o resumo e texto com LF: o modo texto do Windows o dobraria em CRLF */
    return p->resumo ? MapaResumo(q, p, so) : MapaJson(q, so);
}

int RodarMapa(int argc, char **argv)
{
    PedidoConsulta p;
    bool json = false;
    const char *motivo = LerArgumentos(argc, argv, &p, &json);

    if (motivo != NULL) return UsoErrado(motivo);
    return ConsultaExecutar(&p, MapaResponder);
}
