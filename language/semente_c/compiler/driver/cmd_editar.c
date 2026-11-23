/* cmd_editar.c - 'forja editar <arq.fj | pasta> --ops <ops.json> [--aplicar]' (ver cmd_editar.h): le as operacoes, monta o plano de edicoes, confere numa copia e grava.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I5: inserir_decl, remover_decl e mover_decl na tabela de operacoes; o alvo pode vir de ate 4 chaves e o texto e opcional (remover_decl nao tem); a operacao em curso
 *            fica em Plano.item para as chaves extras (depois_de, forcar...). O resto (plano, check na copia, gravacao, resposta) e o do I4.
 *            0.1.0 - IA1-I4: primeira versao. O projeto e compilado e indexado pelo mesmo ConsultaExecutar do 'forja consultar'; as edicoes passam pelo motor edicoes.c (o mesmo do 'forja corrigir');
 *            o "depois" e o "forja check" numa copia do projeto, entao dry-run e edicao recusada nao tocam arquivo nenhum; a gravacao e atomica por arquivo e desfeita se um deles falha.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache/entradas_ler.h"
#include "driver/cmd_editar.h"
#include "driver/editar_interno.h"

typedef struct { const char *alvo, *ops; bool aplicar; } Pedido;

/* Uma operacao do ops.json: nome, as chaves que podem trazer o alvo (a 1a presente vale), a chave do texto (NULL = a operacao nao tem texto) e a funcao. */
typedef struct {
    const char *nome;
    const char *chaves_alvo[4];
    const char *chave_texto;
    bool (*executar)(Plano *p, const char *alvo, const char *texto);
} TipoDeOperacao;

static const TipoDeOperacao OPERACOES[] = {
    {"renomear", {"id", "nome"}, "novo", EditarRenomear},
    {"trocar_no", {"id", NULL}, "texto", EditarTrocarNo},
    {"trocar_corpo", {"id", "proc"}, "texto", EditarTrocarCorpo},
    {"inserir_decl", {"modulo", "arquivo", "depois_de", "antes_de"}, "texto", EditarInserirDecl},
    {"remover_decl", {"id", NULL}, NULL, EditarRemoverDecl},
    {"mover_decl", {"id", NULL}, "para", EditarMoverDecl},
};

typedef struct { const Pedido *pedido; const JsonValor *ops; } Contexto;

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: editar: %s\n", motivo);
    fputs("uso: forja editar [<arq.fj | pasta>] --ops <ops.json> [--aplicar]\n", stderr);
    return EDITAR_USO;
}

static const char *LerArgumentos(int argc, char **argv, Pedido *p)
{
    memset(p, 0, sizeof *p);
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--aplicar") == 0) {
            p->aplicar = true;
        } else if (strcmp(argv[k], "--ops") == 0) {
            if (k + 1 >= argc) return "--ops exige o arquivo das operacoes (ex.: --ops ops.json)";
            p->ops = argv[++k];
        } else if (argv[k][0] == '-') {
            return "opcao desconhecida (valem --ops e --aplicar)";
        } else if (p->alvo != NULL) {
            return "so um alvo por vez";
        } else {
            p->alvo = argv[k];
        }
    }
    return p->ops == NULL ? "falta --ops <ops.json>" : NULL;
}

/* ---------- ops.json ---------- */

static const TipoDeOperacao *AcharTipo(const char *nome)
{
    for (size_t k = 0; nome != NULL && k < sizeof OPERACOES / sizeof OPERACOES[0]; k++)
        if (strcmp(OPERACOES[k].nome, nome) == 0) return &OPERACOES[k];
    return NULL;
}

static const char *AlvoDaOperacao(const JsonValor *item, const TipoDeOperacao *t)
{
    for (int k = 0; k < 4; k++) {
        const char *alvo = t->chaves_alvo[k] != NULL ? JsonTextoDe(item, t->chaves_alvo[k]) : NULL;

        if (alvo != NULL) return alvo;
    }
    return NULL;
}

/* A operacao do ops.json esta completa? Devolve o motivo (em 'motivo') ou NULL. */
static const char *ValidarOperacao(const JsonValor *item, char *motivo, size_t tam, uint32_t numero)
{
    const TipoDeOperacao *t = item->tipo == JSON_OBJETO ? AcharTipo(JsonTextoDe(item, "op")) : NULL;

    if (t == NULL) {
        snprintf(motivo, tam, "operacao %u: falta \"op\" ou nao e uma destas: renomear, trocar_no, trocar_corpo, inserir_decl, remover_decl, mover_decl", numero);
        return motivo;
    }
    if (AlvoDaOperacao(item, t) == NULL || (t->chave_texto != NULL && JsonTextoDe(item, t->chave_texto) == NULL)) {
        snprintf(motivo, tam, "operacao %u (%s): faltam \"%s\" e \"%s\"", numero, t->nome, t->chaves_alvo[0], t->chave_texto != NULL ? t->chave_texto : "(sem texto)");
        return motivo;
    }
    return NULL;
}

/* Le e confere o ops.json: a lista de operacoes em *lista. Devolve o motivo do uso errado ou NULL.
 * LIMITE: parametros 5 (caminho de entrada; lista e a saida; motivo+tam e o buffer da mensagem de uso; funcao de uso unico, struct nao ganha clareza). */
static const char *LerOperacoes(Arena *arena, const char *caminho, const JsonValor **lista, char *motivo, size_t tam)
{
    char *dados = NULL;
    size_t tam_dados = 0, bom;
    const JsonValor *doc;
    uint32_t numero = 0;

    if (!ArquivoLerInteiro(caminho, &dados, &tam_dados)) {
        snprintf(motivo, tam, "nao consegui abrir '%s' para leitura", caminho);
        return motivo;
    }
    bom = EdicoesTemBom(dados, tam_dados) ? EDICOES_BOM_TAM : 0;
    doc = JsonLer(arena, dados + bom, tam_dados - bom);
    free(dados);
    if (doc != NULL && doc->tipo == JSON_OBJETO) doc = JsonMembro(doc, "operacoes");
    if (doc == NULL || doc->tipo != JSON_LISTA) return "o arquivo das operacoes tem de ser JSON valido: uma lista de operacoes, ou {\"operacoes\": [...]}";
    for (const JsonValor *item = doc->filho; item != NULL; item = item->prox) {
        if (ValidarOperacao(item, motivo, tam, ++numero) != NULL) return motivo;
    }
    *lista = doc;
    return NULL;
}

/* ---------- plano ---------- */

/* Executa as operacoes na ordem; a 1a que falha deixa a frase (com o numero dela) em p->erro e para. */
static void ExecutarOperacoes(Plano *p, const JsonValor *ops)
{
    uint32_t numero = 0;

    for (const JsonValor *item = ops->filho; item != NULL && p->erro[0] == '\0'; item = item->prox) {
        const TipoDeOperacao *t = AcharTipo(JsonTextoDe(item, "op"));
        const char *alvo = AlvoDaOperacao(item, t), *texto = t->chave_texto != NULL ? JsonTextoDe(item, t->chave_texto) : NULL;
        uint32_t antes = PlanoTotalEdicoes(p);
        char causa[EDITAR_TEXTO_MAX];

        numero++;
        p->item = item;
        if (t->executar(p, alvo, texto)) {
            PlanoRegistrar(p, t->nome, alvo, texto, antes);
            continue;
        }
        snprintf(causa, sizeof causa, "%s", p->erro);
        snprintf(p->erro, sizeof p->erro, "operacao %u (%s): %.300s", numero, t->nome, causa);
    }
}

/* ---------- conferir e gravar ---------- */

/* O texto novo de cada arquivo (BOM e demais bytes iguais, so as edicoes mudam). false = duas operacoes tocam o mesmo trecho (erro em p->erro). */
static bool MontarTextos(Plano *p, Buf *novos)
{
    for (uint32_t k = 0; k < p->narquivos; k++) {
        const ArquivoEditado *a = &p->arquivos[k];

        if (!EdicoesMontarArquivo(a->original, a->tam, a->edicoes, a->n, &novos[k])) return PlanoErro(p, "duas operacoes tocam o mesmo trecho de '%s'", a->relativo);
    }
    return true;
}

static void MontarDiff(const Plano *p, Buf *diff)
{
    for (uint32_t k = 0; k < p->narquivos; k++) {
        const ArquivoEditado *a = &p->arquivos[k];
        size_t bom = EdicoesTemBom(a->original, a->tam) ? EDICOES_BOM_TAM : 0;

        EdicoesDiff(a->original + bom, a->tam - bom, a->edicoes, a->n, a->relativo, diff);
    }
}

/* Grava os textos novos, um arquivo por vez e de forma atomica; se um falha, devolve os ja gravados ao original. false = erro em p->erro. */
static bool GravarTextos(Plano *p, const Buf *novos)
{
    for (uint32_t k = 0; k < p->narquivos; k++) {
        if (ArquivoGravarAtomico(p->arquivos[k].caminho, novos[k].dados, novos[k].tam, NULL)) continue;
        for (uint32_t j = 0; j < k; j++) EdicoesDesfazer(p->arquivos[j].caminho, p->arquivos[j].original, p->arquivos[j].tam);
        return PlanoErro(p, "nao consegui gravar '%s' (os arquivos ja gravados foram devolvidos ao original)", p->arquivos[k].relativo);
    }
    return true;
}

/* O diagnostico do check da copia que justifica recusar a edicao (o 1o). */
static void MotivoDaRecusa(const Plano *p, const Resposta *r, char *motivo, size_t tam)
{
    const DiagLido *d = &r->depois->itens[0];
    const char *arquivo = d->arquivo != NULL ? d->arquivo : "?";
    size_t n = r->prefixo_copia != NULL ? strlen(r->prefixo_copia) : 0;

    (void)p;
    if (n > 0 && strncmp(arquivo, r->prefixo_copia, n) == 0) arquivo += n;
    snprintf(motivo, tam, "a edicao deixaria %u erro(s) no projeto (que passava limpo): %s em %s:%u; nenhum arquivo foi alterado", r->depois->n, d->codigo, arquivo, d->linha);
}

typedef struct { Plano *plano; const Pedido *pedido; Buf *novos; Buf diff; ResultadoCheck depois; Resposta resposta; char motivo[EDITAR_TEXTO_MAX]; } Conferencia;

/* Roda o check da copia e decide se a edicao vale; grava se pedido. Devolve o exit. */
static int ConferirEGravar(Conferencia *cf, Arena *arena)
{
    Plano *p = cf->plano;
    Resposta *r = &cf->resposta;
    const char *prefixo = NULL;

    *r = (Resposta){.alvo = cf->pedido->alvo != NULL ? cf->pedido->alvo : ".", .aplicar = cf->pedido->aplicar, .erros_depois = -1, .diff = &cf->diff};
    if (PlanoTotalEdicoes(p) == 0) {
        r->valido = true;
        return EDITAR_OK;
    }
    if (!EditarVerificarCopia(p, cf->novos, arena, &cf->depois, &prefixo)) {
        r->motivo = p->erro;
        return EDITAR_INTERNO;
    }
    r->prefixo_copia = prefixo;
    r->depois = &cf->depois;
    r->erros_depois = cf->depois.n;
    r->valido = cf->depois.n == 0;
    if (!r->valido) {
        MotivoDaRecusa(p, r, cf->motivo, sizeof cf->motivo);
        r->motivo = cf->motivo;
        return EDITAR_RECUSADO;
    }
    if (cf->pedido->aplicar && !GravarTextos(p, cf->novos)) {
        r->motivo = p->erro;
        return EDITAR_INTERNO;
    }
    r->aplicado = cf->pedido->aplicar;
    return EDITAR_OK;
}

/* Operacoes ja no plano: monta os textos e o diff, confere e responde. */
static int Concluir(Plano *p, const Pedido *pedido, Arena *arena)
{
    Conferencia cf = {.plano = p, .pedido = pedido};
    int codigo;

    PlanoOrdenar(p);
    cf.novos = calloc(p->narquivos > 0 ? p->narquivos : 1, sizeof *cf.novos);
    if (cf.novos == NULL) FalhaInterna("memoria esgotada montando os textos novos");
    if (MontarTextos(p, cf.novos)) {
        MontarDiff(p, &cf.diff);
        codigo = ConferirEGravar(&cf, arena);
    } else {
        cf.resposta = (Resposta){.alvo = pedido->alvo != NULL ? pedido->alvo : ".", .aplicar = pedido->aplicar, .erros_depois = -1, .motivo = p->erro};
        codigo = EDITAR_RECUSADO;
    }
    EditarEmitirResposta(p, &cf.resposta);
    if (cf.resposta.motivo != NULL) fprintf(stderr, "forja: editar: %s\n", cf.resposta.motivo);
    for (uint32_t k = 0; k < p->narquivos; k++) BufLiberar(&cf.novos[k]);
    free(cf.novos);
    BufLiberar(&cf.diff);
    return codigo;
}

/* O projeto compilou sem erro (Consulta pronta): aplica as operacoes e responde. */
static int Responder(Consulta *q, const PedidoConsulta *pc)
{
    const Contexto *ctx = pc->contexto;
    Arena arena = {0};
    Plano plano = {.q = q, .arena = &arena};
    int codigo;

    ExecutarOperacoes(&plano, ctx->ops);
    if (plano.erro[0] == '\0') {
        codigo = Concluir(&plano, ctx->pedido, &arena);
    } else {
        Resposta r = {.alvo = ctx->pedido->alvo != NULL ? ctx->pedido->alvo : ".", .aplicar = ctx->pedido->aplicar, .erros_depois = -1, .motivo = plano.erro};

        EditarEmitirResposta(&plano, &r);
        fprintf(stderr, "forja: editar: %s\n", plano.erro);
        codigo = EDITAR_RECUSADO;
    }
    PlanoLiberar(&plano);
    ArenaLiberar(&arena);
    return codigo;
}

int RodarEditar(int argc, char **argv)
{
    Pedido pedido;
    Arena arena = {0};
    const JsonValor *ops = NULL;
    char motivo[EDITAR_TEXTO_MAX];
    const char *erro = LerArgumentos(argc, argv, &pedido);
    Contexto ctx = {.pedido = &pedido};
    PedidoConsulta pc = {.pergunta = "editar"};
    int codigo;

    if (erro == NULL) erro = LerOperacoes(&arena, pedido.ops, &ops, motivo, sizeof motivo);
    if (erro != NULL) {
        ArenaLiberar(&arena);
        return UsoErrado(erro);
    }
    ctx.ops = ops;
    pc.alvo = pedido.alvo;
    pc.contexto = &ctx;
    codigo = ConsultaExecutar(&pc, Responder);
    ArenaLiberar(&arena);
    return codigo;
}
