/* cmd_consultar.c - 'forja consultar <pergunta> ...' (ver cmd_consultar.h): le os argumentos, compila o projeto e despacha a pergunta.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6b P79 (D0653-2): 'consultar api std:<m>' e 'vendor:<m>' sem <alvo> monta a raiz sintetica em memoria (EhApiSemHost, AbrirRaizDeApi); sem arquivo hospedeiro.
 *            0.2.0 - IA1-I3: as perguntas callers, callees, imports, api, testes e entrada; a compilacao + indice saem de Executar para ConsultaExecutar (o 'forja mapa' usa a mesma).
 *            0.1.0 - IA1-I2: primeira versao. Despacho por tabela de perguntas; a compilacao e a mesma do check (sintaxe com modulos + semantica) e o indice dos usos sai da mesma
 *            tabela de ligacoes do 'forja ast' (frontend\indice.c).
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "driver/cmd_consultar.h"
#include "driver/consultar_interno.h"

typedef struct { const char *nome, *uso; bool com_arg; FuncaoResposta responder; } Pergunta;

static const Pergunta PERGUNTAS[] = {
    {"simbolos", "simbolos [<alvo>] [--modulo <caminho>]", false, ConsultarSimbolos},
    {"escopo", "escopo <arq:L:C> [<alvo>]", true, ConsultarEscopo},
    {"refs", "refs <nome> [<alvo>]", true, ConsultarRefs},
    {"tipo", "tipo <arq:L:C> [<alvo>]", true, ConsultarTipo},
    {"assinatura", "assinatura <nome> [<alvo>]", true, ConsultarAssinatura},
    {"campos", "campos <Tipo> [<alvo>]", true, ConsultarCampos},
    {"callers", "callers <nome> [<alvo>]", true, ConsultarCallers},
    {"callees", "callees <nome> [<alvo>]", true, ConsultarCallees},
    {"imports", "imports [<alvo>]", false, ConsultarImports},
    {"api", "api <modulo> [<alvo>]", true, ConsultarApi},
    {"testes", "testes [<alvo>]", false, ConsultarTestes},
    {"entrada", "entrada [<alvo>]", false, ConsultarEntrada},
};
enum { N_PERGUNTAS = sizeof PERGUNTAS / sizeof PERGUNTAS[0], POSICIONAIS_MAX = 3, ENTRADA_TAM = MAX_PATH + 16 };

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: consultar: %s\nperguntas:\n", motivo);
    for (int i = 0; i < N_PERGUNTAS; i++) fprintf(stderr, "     forja consultar %s [--json]\n", PERGUNTAS[i].uso);
    return CONSULTAR_USO;
}

static const Pergunta *AcharPergunta(const char *nome)
{
    for (int i = 0; i < N_PERGUNTAS; i++) {
        if (strcmp(PERGUNTAS[i].nome, nome) == 0) return &PERGUNTAS[i];
    }
    return NULL;
}

typedef struct {
    const char *itens[POSICIONAIS_MAX];
    int n;
} Posicionais;

/* Separa opcoes e posicionais; devolve o motivo do uso errado ou NULL. */
static const char *LerPosicionais(int argc, char **argv, PedidoConsulta *p, Posicionais *pos)
{
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--json") == 0) continue;
        if (strcmp(argv[k], "--modulo") == 0) {
            if (k + 1 >= argc) return "--modulo exige o caminho do modulo (ex.: --modulo std:mem)";
            p->modulo = argv[++k];
        } else if (strncmp(argv[k], "--", 2) == 0) {
            return "opcao desconhecida (valem --json e, em simbolos, --modulo)";
        } else if (pos->n < POSICIONAIS_MAX) {
            pos->itens[pos->n++] = argv[k];
        } else {
            return "argumentos demais";
        }
    }
    return NULL;
}

static const char *LerPedido(int argc, char **argv, PedidoConsulta *p, const Pergunta **pergunta)
{
    Posicionais pos = {.n = 0};
    const char *motivo;
    int n, prox = 1;

    memset(p, 0, sizeof *p);
    motivo = LerPosicionais(argc, argv, p, &pos);
    if (motivo != NULL) return motivo;
    n = pos.n;
    if (n == 0) return "falta a pergunta";
    *pergunta = AcharPergunta(pos.itens[0]);
    if (*pergunta == NULL) return "pergunta desconhecida";
    p->pergunta = (*pergunta)->nome;
    if ((*pergunta)->com_arg) {
        if (n < 2) return "falta o argumento da pergunta";
        p->arg = pos.itens[prox++];
    }
    if (n > prox) p->alvo = pos.itens[prox++];
    if (n > prox) return "argumentos demais";
    if (p->modulo != NULL && strcmp(p->pergunta, "simbolos") != 0) return "--modulo so vale em simbolos";
    return NULL;
}

static bool EhPasta(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);
    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/* O .fj raiz: o <alvo> (pasta = main.fj dela), ou o arquivo da posicao em escopo/tipo, ou o main.fj da pasta atual. */
static void ResolverEntrada(const PedidoConsulta *p, char *entrada)
{
    char base[MAX_PATH];
    bool por_posicao = strcmp(p->pergunta, "escopo") == 0 || strcmp(p->pergunta, "tipo") == 0;

    if (p->alvo != NULL) snprintf(base, sizeof base, "%s", p->alvo);
    else if (!por_posicao || !ConsultaArquivoDaPosicao(p->arg, base, sizeof base)) snprintf(base, sizeof base, ".");
    if (EhPasta(base)) snprintf(entrada, ENTRADA_TAM, "%s\\main.fj", base);
    else snprintf(entrada, ENTRADA_TAM, "%s", base);
}

/* Projeto ja analisado sem erro: monta o indice e as posicoes finais, responde e escreve a linha JSON. */
static int ResponderProjeto(Compilacao *c, const PedidoConsulta *p, FuncaoResposta responder)
{
    Consulta q = {.c = c};
    int codigo;

    IndiceMontar(&q.indice, &c->sem);
    if (!AstFimIniciar(&q.fim, &c->fonte, &c->nomes, &c->diag)) {
        IndiceLiberar(&q.indice);
        fputs("forja: consultar: erro lexico ao reler o fonte para as posicoes finais\n", stderr);
        return CONSULTAR_INTERNO;
    }
    codigo = responder(&q, p);
    if (codigo == CONSULTAR_OK) fwrite(q.saida.dados, 1, q.saida.tam, stdout);
    BufLiberar(&q.saida);
    AstFimLiberar(&q.fim);
    IndiceLiberar(&q.indice);
    return codigo;
}

/* D0653-2 (P79): 'consultar api std:<m>' ou 'vendor:<m>' sem <alvo> nao tem arquivo hospedeiro. */
static bool EhApiSemHost(const PedidoConsulta *p)
{
    return strcmp(p->pergunta, "api") == 0 && p->alvo == NULL && (strncmp(p->arg, "std:", 4) == 0 || strncmp(p->arg, "vendor:", 7) == 0);
}

/* A raiz sintetica do modulo, em memoria, sob <pasta atual>\consultar_api.fj (como o forja test): so importa o modulo; o alvo leva o import sem E1005.
 * entrada guarda o caminho virtual (a Fonte aponta para ele); false = memoria esgotada ou pasta atual longa demais. */
static bool AbrirRaizDeApi(const PedidoConsulta *p, char *entrada, Compilacao *c, Opcoes *o)
{
    char raiz[256];
    int n;
    DWORD pasta;

    pasta = GetCurrentDirectoryA(ENTRADA_TAM, entrada);
    if (pasta == 0 || pasta + sizeof "\\consultar_api.fj" > ENTRADA_TAM) return false;
    snprintf(entrada + pasta, ENTRADA_TAM - pasta, "\\consultar_api.fj");
    n = snprintf(raiz, sizeof raiz, "import \"%s\" as alvo_teste\nproc main() {\n}\n", p->arg);
    if (n < 0 || (size_t)n >= sizeof raiz) return false;
    o->entrada = entrada;
    o->alvo_teste = p->arg;
    return FonteAdotarTexto(&c->fonte, entrada, raiz, (size_t)n);
}

int ConsultaExecutar(const PedidoConsulta *p, FuncaoResposta responder)
{
    char entrada[ENTRADA_TAM];
    Opcoes o = {.comando = "check"};
    Compilacao *c = calloc(1, sizeof *c);
    int codigo;

    if (c == NULL) FalhaInterna("memoria esgotada alocando a compilacao");
    if (EhApiSemHost(p)) {
        if (!AbrirRaizDeApi(p, entrada, c, &o)) {
            free(c);
            return CONSULTAR_USO;
        }
    } else {
        ResolverEntrada(p, entrada);
        o.entrada = entrada;
        if (!FonteLer(&c->fonte, entrada)) {
            free(c);
            return CONSULTAR_USO;
        }
    }
    IniciarCompilacao(c);
    c->opcoes = &o;
    c->sem.lista.teto = OpcoesMaxErros(&o);
    if (!AnalisarSintaxe(c, true) || !AnalisarSemantica(c)) codigo = RelatarFalha(c);
    else codigo = ResponderProjeto(c, p, responder);
    LiberarCompilacao(c);
    free(c);
    return codigo;
}

int RodarConsultar(int argc, char **argv)
{
    PedidoConsulta p;
    const Pergunta *pergunta = NULL;
    const char *motivo = LerPedido(argc, argv, &p, &pergunta);

    if (motivo != NULL) return UsoErrado(motivo);
    return ConsultaExecutar(&p, pergunta->responder);
}
