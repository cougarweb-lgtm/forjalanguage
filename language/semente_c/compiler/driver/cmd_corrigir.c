/* cmd_corrigir.c - 'forja corrigir <arq.fj> [--codigo X] [--aplicar] [--tambem-provavel]' (ver cmd_corrigir.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I4: BOM, montar o arquivo, desfazer e a escrita JSON de edicao foram para driver\edicoes.c (o 'forja editar' usa o mesmo motor); comportamento e saida iguais.
 *            0.1.0 - IA1-b05: primeira versao. O check roda num filho (corrigir_diag.c), as edicoes passam pelo motor edicoes.c, a gravacao e atomica e o arquivo volta
 *            byte a byte se o check de depois e pior que o de antes. Uma sugestao entra inteira ou nao entra (as edicoes dela sao um conjunto).
 * Verificacao: construir.bat && python tests\ia_first\verificar_corrigir.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache/entradas_ler.h"
#include "common/diag.h"
#include "driver/cmd_corrigir.h"
#include "driver/corrigir_diag.h"
#include "nativo/nativo_json.h"

enum { SAIDA_OK = 0, SAIDA_DESFEITO = 1, SAIDA_USO = 2, SAIDA_INTERNO = 3, TEXTO_MAX = 400 };

typedef struct { const char *arquivo, *codigo; bool aplicar, provavel; } Pedido;

/* O que o corrigir vai aplicar: as sugestoes escolhidas e a uniao das edicoes delas (sem repetidas). */
typedef struct {
    const DiagLido **diags;
    uint32_t ndiags;
    Edicao *edicoes;
    uint32_t nedicoes;
    uint32_t puladas;           /* sugestoes boas que ficaram de fora por tocar o mesmo trecho de outra */
} Selecao;

/* O que aconteceu com o arquivo. */
typedef struct {
    bool aplicado, desfeito;
    int64_t erros_depois;       /* -1 = o check de depois nao rodou */
    char motivo[TEXTO_MAX];     /* "" = nenhum */
    Buf diff;
} Efeito;

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: corrigir: %s\n", motivo);
    fputs("uso: forja corrigir <arq.fj> [--codigo CODIGO] [--aplicar] [--tambem-provavel]\n", stderr);
    return SAIDA_USO;
}

/* --codigo CODIGO: *k aponta para a opcao e passa para o valor. Devolve o motivo do uso errado ou NULL. */
static const char *LerCodigo(int argc, char **argv, int *k, Pedido *p)
{
    if (*k + 1 >= argc) return "--codigo exige um valor (ex.: --codigo E0707)";
    p->codigo = argv[++*k];
    if (DiagCodigoPorTexto(p->codigo) < 0) return "codigo de diagnostico desconhecido (veja 'forja explicar')";
    return NULL;
}

static const char *LerArgumentos(int argc, char **argv, Pedido *p)
{
    memset(p, 0, sizeof *p);
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--aplicar") == 0) {
            p->aplicar = true;
            continue;
        }
        if (strcmp(argv[k], "--tambem-provavel") == 0) {
            p->provavel = true;
            continue;
        }
        if (strcmp(argv[k], "--codigo") == 0) {
            const char *motivo = LerCodigo(argc, argv, &k, p);

            if (motivo != NULL) return motivo;
            continue;
        }
        if (argv[k][0] == '-') return "opcao desconhecida";
        if (p->arquivo != NULL) return "so um arquivo por vez";
        p->arquivo = argv[k];
    }
    return p->arquivo == NULL ? "falta o arquivo .fj" : NULL;
}

/* ---------- escolha das correcoes ---------- */

static bool DiagServe(const Pedido *p, const DiagLido *d)
{
    bool confianca = d->confianca != NULL && (strcmp(d->confianca, "certa") == 0 || (p->provavel && strcmp(d->confianca, "provavel") == 0));

    if (!confianca || d->nedicoes == 0 || !d->edicoes_no_alvo) return false;
    return p->codigo == NULL || strcmp(p->codigo, d->codigo) == 0;
}

/* A edicao toca o mesmo trecho de uma ja escolhida sem ser igual a ela? */
static bool ConflitaComEscolhidas(const Selecao *s, const Edicao *e)
{
    for (uint32_t j = 0; j < s->nedicoes; j++)
        if (!EdicoesIguais(e, &s->edicoes[j]) && EdicoesConflitam(e, &s->edicoes[j])) return true;
    return false;
}

static bool JaEscolhida(const Selecao *s, const Edicao *e)
{
    for (uint32_t j = 0; j < s->nedicoes; j++)
        if (EdicoesIguais(e, &s->edicoes[j])) return true;
    return false;
}

/* A sugestao entra inteira (as edicoes repetidas de outra ja escolhida contam como cobertas) ou nao entra. */
static void Juntar(Selecao *s, const DiagLido *d)
{
    for (uint32_t k = 0; k < d->nedicoes; k++) {
        if (ConflitaComEscolhidas(s, &d->edicoes[k])) {
            s->puladas++;
            return;
        }
    }
    for (uint32_t k = 0; k < d->nedicoes; k++)
        if (!JaEscolhida(s, &d->edicoes[k])) s->edicoes[s->nedicoes++] = d->edicoes[k];
    s->diags[s->ndiags++] = d;
}

static void Selecionar(const Pedido *p, const ResultadoCheck *antes, Arena *arena, Selecao *s)
{
    size_t total = 1;

    memset(s, 0, sizeof *s);
    for (uint32_t k = 0; k < antes->n; k++) total += antes->itens[k].nedicoes;
    s->edicoes = ArenaAlocar(arena, total * sizeof *s->edicoes);
    s->diags = ArenaAlocar(arena, ((size_t)antes->n + 1) * sizeof *s->diags);
    for (uint32_t k = 0; k < antes->n; k++)
        if (DiagServe(p, &antes->itens[k])) Juntar(s, &antes->itens[k]);
}

/* ---------- a correcao piorou? ---------- */

/* Etapa do codigo: 0 = sintaxe (E00xx e E01xx), 1 = os demais E, 2 = o resto (R, B, G, X). */
static int EtapaDoCodigo(const char *codigo)
{
    if (codigo[0] != 'E') return 2;
    return codigo[1] == '0' && (codigo[2] == '0' || codigo[2] == '1') ? 0 : 1;
}

static int EtapaMinima(const ResultadoCheck *r)
{
    int minima = 2;

    for (uint32_t k = 0; k < r->n; k++) {
        int etapa = EtapaDoCodigo(r->itens[k].codigo);

        if (etapa < minima) minima = etapa;
    }
    return minima;
}

/* O erro que a sugestao tratava (mesmo codigo, na linha onde ele ficou depois das edicoes) continua no arquivo? */
static const DiagLido *ErroOriginalQueFicou(const Selecao *s, const ResultadoCheck *depois)
{
    for (uint32_t k = 0; k < s->ndiags; k++) {
        uint32_t linha = EdicoesLinhaNova(s->edicoes, s->nedicoes, s->diags[k]->linha);

        for (uint32_t j = 0; j < depois->n; j++) {
            const DiagLido *x = &depois->itens[j];

            if (x->linha == linha && strcmp(x->codigo, s->diags[k]->codigo) == 0) return x;
        }
    }
    return NULL;
}

/* Frase em 'motivo' se o arquivo ficou pior que antes (o contrato M-D2 e M-D13); "" se nao. */
static void MotivoDaPiora(const Selecao *s, const ResultadoCheck *antes, const ResultadoCheck *depois, char *motivo)
{
    const DiagLido *ficou = ErroOriginalQueFicou(s, depois);
    int etapa_antes = EtapaMinima(antes);

    motivo[0] = '\0';
    if (depois->n > antes->n) {
        snprintf(motivo, TEXTO_MAX, "o numero de erros subiu de %u para %u", antes->n, depois->n);
    } else if (ficou != NULL) {
        snprintf(motivo, TEXTO_MAX, "o erro original continua: %s na linha %u", ficou->codigo, ficou->linha);
    }
    for (uint32_t k = 0; k < depois->n && motivo[0] == '\0'; k++) {
        const DiagLido *x = &depois->itens[k];

        if (!CorrigirTemCodigo(antes, x->codigo) && EtapaDoCodigo(x->codigo) < etapa_antes)
            snprintf(motivo, TEXTO_MAX, "apareceu erro de etapa anterior: %s na linha %u", x->codigo, x->linha);
    }
}

/* ---------- gravar, conferir, desfazer ---------- */

typedef struct { const Pedido *pedido; const ResultadoCheck *antes; const Selecao *selecao; Arena *arena; const char *original; size_t tam; } Contexto;

/* Grava as edicoes, roda o check de novo e desfaz se piorou. Devolve o exit do comando. */
static int AplicarEConferir(const Contexto *c, Efeito *ef)
{
    Buf novo = {0};
    ResultadoCheck depois;
    char erro[TEXTO_MAX];
    int codigo = SAIDA_OK;

    if (!EdicoesMontarArquivo(c->original, c->tam, c->selecao->edicoes, c->selecao->nedicoes, &novo)) {
        BufLiberar(&novo);
        snprintf(ef->motivo, TEXTO_MAX, "as edicoes nao se aplicam ao arquivo");
        return SAIDA_INTERNO;
    }
    if (!ArquivoGravarAtomico(c->pedido->arquivo, novo.dados, novo.tam, NULL)) {
        BufLiberar(&novo);
        snprintf(ef->motivo, TEXTO_MAX, "nao consegui gravar %s", c->pedido->arquivo);
        return SAIDA_INTERNO;
    }
    BufLiberar(&novo);
    if (CorrigirRodarCheck(c->arena, c->pedido->arquivo, &depois, erro, sizeof erro)) {
        MotivoDaPiora(c->selecao, c->antes, &depois, ef->motivo);
        ef->erros_depois = depois.n;
    } else {
        snprintf(ef->motivo, TEXTO_MAX, "o check depois da correcao falhou: %.300s", erro);
    }
    ef->aplicado = ef->motivo[0] == '\0';
    if (ef->aplicado) return SAIDA_OK;
    ef->desfeito = true;
    codigo = SAIDA_DESFEITO;
    if (!EdicoesDesfazer(c->pedido->arquivo, c->original, c->tam)) {
        ef->desfeito = false;
        snprintf(ef->motivo, TEXTO_MAX, "GRAVE: nao consegui devolver %s ao conteudo original", c->pedido->arquivo);
        codigo = SAIDA_INTERNO;
    }
    return codigo;
}

/* ---------- saida JSON ---------- */

static void EmitirCorrecao(Buf *b, const DiagLido *d, const char *arquivo)
{
    BufAnexar(b, "{\"id\": ");
    EdicoesJsonTextoOuNulo(b, d->id);
    BufFormatar(b, ", \"codigo\": \"%s\", \"linha\": %u, \"coluna\": %u, \"confianca\": \"%s\", \"sugestao\": ", d->codigo, d->linha, d->coluna, d->confianca);
    EdicoesJsonTextoOuNulo(b, d->sugestao);
    BufAnexar(b, ", \"edicoes\": [");
    for (uint32_t k = 0; k < d->nedicoes; k++) {
        if (k > 0) BufAnexar(b, ", ");
        EdicoesJsonEdicao(b,&d->edicoes[k], arquivo);
    }
    BufAnexar(b, "]}");
}

/* Uma linha JSON, chaves em ordem fixa (M-D6: mesma entrada, mesmos bytes). */
static void EmitirResultado(const Contexto *c, const Efeito *ef)
{
    const Pedido *p = c->pedido;
    Buf b = {0};

    BufAnexar(&b, "{\"comando\": \"corrigir\", \"arquivo\": ");
    JsonCaminho(&b, p->arquivo);
    BufFormatar(&b, ", \"aplicar\": %s, \"codigo\": ", p->aplicar ? "true" : "false");
    EdicoesJsonTextoOuNulo(&b, p->codigo);
    BufFormatar(&b, ", \"confianca\": \"%s\", \"erros_antes\": %u, \"correcoes\": [", p->provavel ? "certa+provavel" : "certa", c->antes->n);
    for (uint32_t k = 0; k < c->selecao->ndiags; k++) {
        if (k > 0) BufAnexar(&b, ", ");
        EmitirCorrecao(&b, c->selecao->diags[k], p->arquivo);
    }
    BufFormatar(&b, "], \"puladas\": %u, \"edicoes\": %u, \"diff\": ", c->selecao->puladas, c->selecao->nedicoes);
    EdicoesJsonTexto(&b, ef->diff.dados != NULL ? ef->diff.dados : "", ef->diff.tam);
    BufFormatar(&b, ", \"aplicado\": %s, \"desfeito\": %s, \"erros_depois\": ", ef->aplicado ? "true" : "false", ef->desfeito ? "true" : "false");
    if (ef->erros_depois < 0) BufAnexar(&b, "null");
    else BufFormatar(&b, "%lld", (long long)ef->erros_depois);
    BufAnexar(&b, ", \"motivo\": ");
    EdicoesJsonTextoOuNulo(&b, ef->motivo);
    BufAnexar(&b, "}\n");
    fwrite(b.dados, 1, b.tam, stdout);
    BufLiberar(&b);
}

/* ---------- comando ---------- */

/* O diff das correcoes escolhidas (sem gravar nada). false = as edicoes nao cabem no arquivo (o check e o arquivo discordam). */
static bool CalcularDiff(const Contexto *c, Efeito *ef)
{
    size_t bom = EdicoesTemBom(c->original, c->tam) ? EDICOES_BOM_TAM : 0;
    char nome[600];
    EdicaoStatus st;

    snprintf(nome, sizeof nome, "%s", c->pedido->arquivo);
    for (char *q = nome; *q != '\0'; q++)
        if (*q == '\\') *q = '/';
    st = EdicoesDiff(c->original + bom, c->tam - bom, c->selecao->edicoes, c->selecao->nedicoes, nome, &ef->diff);
    if (st != EDICAO_OK) snprintf(ef->motivo, TEXTO_MAX, "%s", EdicaoStatusTexto(st));
    return st == EDICAO_OK;
}

static int Executar(const Pedido *p, Arena *arena, const char *original, size_t tam)
{
    ResultadoCheck antes;
    Selecao selecao;
    Efeito ef = {.erros_depois = -1};
    Contexto c = {.pedido = p, .antes = &antes, .selecao = &selecao, .arena = arena, .original = original, .tam = tam};
    char erro[TEXTO_MAX];
    int codigo = SAIDA_OK;

    if (!CorrigirRodarCheck(arena, p->arquivo, &antes, erro, sizeof erro)) {
        fprintf(stderr, "forja: corrigir: %s\n", erro);
        return SAIDA_INTERNO;
    }
    Selecionar(p, &antes, arena, &selecao);
    if (!CalcularDiff(&c, &ef)) {
        fprintf(stderr, "forja: corrigir: %s\n", ef.motivo);
        BufLiberar(&ef.diff);
        return SAIDA_INTERNO;
    }
    if (p->aplicar && selecao.nedicoes > 0) codigo = AplicarEConferir(&c, &ef);
    EmitirResultado(&c, &ef);
    if (ef.motivo[0] != '\0') fprintf(stderr, "forja: corrigir: %s%s\n", ef.desfeito ? "a correcao piorou o arquivo e foi desfeita: " : "", ef.motivo);
    BufLiberar(&ef.diff);
    return codigo;
}

int RodarCorrigir(int argc, char **argv)
{
    Pedido pedido;
    Arena arena = {0};
    char *original = NULL;
    size_t tam = 0;
    const char *erro = LerArgumentos(argc, argv, &pedido);
    int codigo;

    if (erro != NULL) return UsoErrado(erro);
    if (!ArquivoLerInteiro(pedido.arquivo, &original, &tam)) {
        fprintf(stderr, "forja: nao consegui abrir '%s' para leitura\n", pedido.arquivo);
        return SAIDA_USO;
    }
    codigo = Executar(&pedido, &arena, original, tam);
    free(original);
    ArenaLiberar(&arena);
    return codigo;
}
