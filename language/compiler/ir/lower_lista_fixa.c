/* lower_lista_fixa.c - lowering de [dynamic, N]T, a lista de capacidade fixa (v1.11 L15a, A08).
 * O TipoId da lista e a struct oculta {itens: [N]T, n: int} (sem_lista_fixa.c); aqui so as operacoes mudam, sem IrOp nova:
 *   len(l)      = IR_CAMPO n
 *   l[i]        = IR_ELEMENTO / IR_ENDERECO_ELEMENTO sobre a fatia itens[0 ..< n]: dev falha R0006 com i fora de 0..<n (o texto de forja_indice, o mesmo da
 *                 lista e do array); release nao confere, como o array
 *   l[..], l[a..<b] = a fatia itens[0 ..< n] (limites provados) e a fatia dela; for / for & / for i = o laco 0..<n sobre essa fatia
 *   append      = n >= N: R0007 "append numa [dynamic, N]T cheia (N)" pelo servico forja_falha_alocacao do runtime (mesmo texto livre das outras falhas de
 *                 alocacao; vale em dev e em release); senao itens[n] = x e n += 1
 *   {} / l = {} = a struct zerada (LwZeroDoTipo)
 * O servico entra como IrExterno interno "0lista_cheia" (simbolo forja_falha_alocacao: o BIR chama o simbolo do rt nativo),
 * criado na 1a vez que um append precisa dele.
 * Interface (lower_interno.h): LwEhListaFixa, LwIndiceListaFixa, LwEnderecoListaFixa, LwFatiaListaFixa, LwTamanhoListaFixa, LwBaseListaFixa e LwAppendListaFixa.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.11 L15a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"
#include "semantic/sem_interno.h"

#include <stdio.h>
#include <string.h>

static const char *const NOME_DO_SERVICO = "0lista_cheia";
static const char *const SIMBOLO_DO_SERVICO = "forja_falha_alocacao";
enum { PARAMS_DO_SERVICO = 4 };

bool LwEhListaFixa(const LowerCtx *c, TipoId t)
{
    bool por_ponteiro;
    return TipoListaFixaDe(&c->sem->tipos, t, &por_ponteiro) != 0;
}

/* A lista em si (nao o ponteiro) de um tipo [dynamic, N]T ou ^[dynamic, N]T. */
static TipoId ListaDoTipo(const LowerCtx *c, TipoId t)
{
    bool por_ponteiro;
    return TipoListaFixaDe(&c->sem->tipos, t, &por_ponteiro);
}

static IrInstr *InstrComTemp(LowerCtx *c, IrOp op, uint32_t pos, TipoId tipo)
{
    IrInstr *i = LwNovaInstr(c, op, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    return i;
}

/* ^Lista da expressao: ponteiro pelo valor; lvalor pelo endereco; rvalue (retorno de proc) num local sintetico. */
static Valor PonteiroDaLista(LowerCtx *c, const Expr *l)
{
    if (TipoClasseDe(&c->sem->tipos, l->tipo) == TC_PONTEIRO) return LwExpr(c, l);
    if (LwEnderecavel(c, l)) return LwEndereco(c, l);
    Valor valor = LwExpr(c, l);
    int32_t local = LwNovoLocal(c);
    IrInstr *definicao = LwNovaInstr(c, IR_LOCAL, l->pos_op);
    definicao->destino = local;
    definicao->tipo = l->tipo;
    definicao->mutavel = true;
    definicao->a = valor;
    IrInstr *endereco = InstrComTemp(c, IR_UNARIO, l->pos_op, TipoPonteiro(&c->sem->tipos, l->tipo));
    endereco->op_aux = OP_ENDERECO;
    endereco->a = LwValorLocal(local, l->tipo);
    return LwValorTemp(endereco->destino, endereco->tipo);
}

/* n: o campo n da lista apontada por 'ponteiro'. */
static Valor LerN(LowerCtx *c, Valor ponteiro, uint32_t pos)
{
    IrInstr *i = InstrComTemp(c, IR_CAMPO, pos, TIPO_INT);
    i->a = ponteiro;
    i->nome = c->sem->nome_n;
    return LwValorTemp(i->destino, TIPO_INT);
}

/* ^[N]T do campo itens da lista apontada por 'ponteiro'. */
static Valor EnderecoDosItens(LowerCtx *c, Valor ponteiro, TipoId lista, uint32_t pos)
{
    TipoId tipo = TipoPonteiro(&c->sem->tipos, TipoListaFixaArray(&c->sem->tipos, lista));
    IrInstr *i = InstrComTemp(c, IR_ENDERECO_CAMPO, pos, tipo);
    i->a = ponteiro;
    i->nome = c->sem->nome_itens;
    return LwValorTemp(i->destino, tipo);
}

/* itens[0 ..< n] como []T: n <= N sempre, entao os limites sao provados (op_aux 1) e o R0006 de l[i] vem do IR_ELEMENTO sobre essa fatia. */
static Valor FatiaDosItens(LowerCtx *c, Valor ponteiro, TipoId lista, uint32_t pos)
{
    Valor n = LerN(c, ponteiro, pos);
    Valor itens = EnderecoDosItens(c, ponteiro, lista, pos);
    IrInstr *i = InstrComTemp(c, IR_FATIA, pos, TipoFatia(&c->sem->tipos, TipoListaFixaElemento(&c->sem->tipos, lista)));
    i->a = itens;
    i->b = LwValorInt(0, TIPO_INT);
    i->args = LwArgs(c, 1);
    i->args[0] = n;
    i->nargs = 1;
    i->faixa_excl = true;
    i->op_aux = 1;
    return LwValorTemp(i->destino, i->tipo);
}

/* A fatia dos itens da lista da expressao 'l' (base de l[i], &l[i], l[a..<b], l[..] e do for). */
static Valor FatiaDaExpressao(LowerCtx *c, const Expr *l)
{
    Valor ponteiro = PonteiroDaLista(c, l);
    return FatiaDosItens(c, ponteiro, ListaDoTipo(c, l->tipo), l->pos_op);
}

Valor LwIndiceListaFixa(LowerCtx *c, const Expr *e)
{
    Valor base = FatiaDaExpressao(c, e->a);
    Valor indice = LwIndiceInt(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_ELEMENTO, e->pos_op, e->tipo);
    i->a = base;
    i->b = indice;
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwEnderecoListaFixa(LowerCtx *c, const Expr *e)
{
    Valor base = FatiaDaExpressao(c, e->a);
    Valor indice = LwIndiceInt(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_ENDERECO_ELEMENTO, e->pos_op, TipoPonteiro(&c->sem->tipos, e->tipo));
    i->a = base;
    i->b = indice;
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwFatiaListaFixa(LowerCtx *c, const Expr *e)
{
    Valor base = FatiaDaExpressao(c, e->a);
    if (e->op == FATIA_INTEIRA) return base;
    Valor inicio = LwIndiceInt(c, e->args[0].valor);
    Valor fim = LwIndiceInt(c, e->args[1].valor);
    IrInstr *i = InstrComTemp(c, IR_FATIA, e->pos_op, e->tipo);
    i->a = base;
    i->b = inicio;
    i->args = LwArgs(c, 1);
    i->args[0] = fim;
    i->nargs = 1;
    i->faixa_excl = e->op == FATIA_EXCL;
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwTamanhoListaFixa(LowerCtx *c, const Expr *arg)
{
    return LerN(c, PonteiroDaLista(c, arg), arg->pos_op);
}

Valor LwBaseListaFixa(LowerCtx *c, const Expr *e, Valor *tamanho)
{
    Valor ponteiro = PonteiroDaLista(c, e);
    TipoId lista = ListaDoTipo(c, e->tipo);
    *tamanho = LerN(c, ponteiro, e->pos_op);
    return FatiaDosItens(c, ponteiro, lista, e->pos_op);
}

/* ---------- append ---------- */

/* O servico de falha de alocacao como IrExterno interno (criado na 1a vez; o vetor de externos tem uma vaga a mais, ver LowerPrograma). */
static int32_t NomeDoServico(LowerCtx *c)
{
    IrPrograma *p = c->prog;
    int32_t nome = Internar(c->sem->nomes, NOME_DO_SERVICO, (uint32_t)strlen(NOME_DO_SERVICO));
    for (uint32_t k = 0; k < p->nexternos; k++) {
        if (p->externos[k].nome == nome) return nome;
    }
    static const char *const PARAMETROS[PARAMS_DO_SERVICO] = {"oque", "valor", "arquivo", "linha"};
    const TipoId tipos[PARAMS_DO_SERVICO] = {TIPO_CSTR, TIPO_INT, TIPO_CSTR, TIPO_I32};
    IrExterno *e = &p->externos[p->nexternos++];
    e->nome = nome;
    e->simbolo = Internar(c->sem->nomes, SIMBOLO_DO_SERVICO, (uint32_t)strlen(SIMBOLO_DO_SERVICO));
    e->retorno = TIPO_VAZIO;
    e->nparams = PARAMS_DO_SERVICO;
    e->params = LwAlocar(c, PARAMS_DO_SERVICO * sizeof(IrParam));
    for (uint32_t k = 0; k < PARAMS_DO_SERVICO; k++) {
        e->params[k] = (IrParam){Internar(c->sem->nomes, PARAMETROS[k], (uint32_t)strlen(PARAMETROS[k])), tipos[k]};
    }
    e->interno = true;
    return nome;
}

static Valor TextoCstr(LowerCtx *c, const char *texto)
{
    return LwValorTexto(Internar(c->sem->nomes, texto, (uint32_t)strlen(texto)), TIPO_CSTR);
}

/* append numa lista cheia: R0007 com a causa "append numa [dynamic, 64]Bala cheia" e o valor N, na linha do append. */
static void FalharCheia(LowerCtx *c, TipoId lista, uint32_t pos)
{
    char tipo[96], oque[160];
    SemDescrever(c->sem, lista, tipo, sizeof tipo);
    snprintf(oque, sizeof oque, "append numa %s cheia", tipo);
    FontePosicao onde = FonteLocalizarNoArquivo(c->sem->fonte, pos);
    IrInstr *i = LwNovaInstr(c, IR_CHAMADA, pos);
    i->nome = NomeDoServico(c);
    i->tipo = TIPO_VAZIO;
    i->args = LwArgs(c, PARAMS_DO_SERVICO);
    i->args[0] = TextoCstr(c, oque);
    i->args[1] = LwValorInt((int64_t)TipoListaFixaN(&c->sem->tipos, lista), TIPO_INT);
    i->args[2] = TextoCstr(c, onde.caminho);
    i->args[3] = LwValorInt((int64_t)onde.linha, TIPO_I32);
    i->nargs = PARAMS_DO_SERVICO;
}

/* n >= N (bool). */
static Valor CompararComCapacidade(LowerCtx *c, Valor n, int64_t capacidade, uint32_t pos)
{
    IrInstr *i = InstrComTemp(c, IR_BINARIO, pos, TIPO_BOOL);
    i->op_aux = OP_MAIOR_IGUAL;
    i->a = n;
    i->b = LwValorInt(capacidade, TIPO_INT);
    return LwValorTemp(i->destino, TIPO_BOOL);
}

/* n + 1 (int). */
static Valor MaisUm(LowerCtx *c, Valor n, uint32_t pos)
{
    IrInstr *i = InstrComTemp(c, IR_BINARIO, pos, TIPO_INT);
    i->op_aux = OP_SOMA;
    i->a = n;
    i->b = LwValorInt(1, TIPO_INT);
    return LwValorTemp(i->destino, TIPO_INT);
}

/* O que o append grava: a lista (tipo), o ponteiro para ela, o n lido antes e o item. */
typedef struct { TipoId lista; Valor ponteiro, n, item; } ItemDaLista;

/* itens[n] = item e n += 1 (n < N provado pelo desvio anterior). */
static void GravarItem(LowerCtx *c, const ItemDaLista *gravar, uint32_t pos)
{
    const TipoId lista = gravar->lista;
    const Valor ponteiro = gravar->ponteiro, n = gravar->n, item = gravar->item;
    TipoId elemento = TipoListaFixaElemento(&c->sem->tipos, lista);
    Valor itens = EnderecoDosItens(c, ponteiro, lista, pos);
    IrInstr *destino = InstrComTemp(c, IR_ENDERECO_ELEMENTO, pos, TipoPonteiro(&c->sem->tipos, elemento));
    destino->a = itens;
    destino->b = n;
    destino->op_aux = 1;
    IrInstr *grava = LwNovaInstr(c, IR_GRAVA_PONTEIRO, pos);
    grava->tipo = elemento;
    grava->a = LwValorTemp(destino->destino, destino->tipo);
    grava->b = item;
    grava->nome = -1;
    Valor novo_n = MaisUm(c, n, pos);
    IrInstr *grava_n = LwNovaInstr(c, IR_GRAVA_PONTEIRO, pos);
    grava_n->tipo = TIPO_INT;
    grava_n->a = ponteiro;
    grava_n->b = novo_n;
    grava_n->nome = c->sem->nome_n;
}

Valor LwAppendListaFixa(LowerCtx *c, const Expr *e, const Expr *alvo, const Expr *item_expr)
{
    TipoId lista = ListaDoTipo(c, alvo->tipo);
    Valor ponteiro = LwExpr(c, alvo);
    Valor item = LwExpr(c, item_expr);
    Valor n = LerN(c, ponteiro, e->pos_op);
    Valor cheia = CompararComCapacidade(c, n, (int64_t)TipoListaFixaN(&c->sem->tipos, lista), e->pos_op);
    IrInstr *se = LwNovaInstr(c, IR_SE, e->pos_op);
    se->a = cheia;
    IrBloco *anterior = LwTrocarBloco(c, &se->corpo);
    FalharCheia(c, lista, e->pos_op);
    LwTrocarBloco(c, anterior);
    ItemDaLista gravar = {.lista = lista, .ponteiro = ponteiro, .n = n, .item = item};
    GravarItem(c, &gravar, e->pos_op);
    return LwValorNenhum();
}
