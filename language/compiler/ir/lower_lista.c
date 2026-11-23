/* lower_lista.c - lowering da lista dona (IA1-L9c): cap, clear, reserve, truncate, pop, insert_at, ordered_remove, unordered_remove, remove_value; e as pecas de IR
 * (Ll*) que lower_busca.c tambem usa. Sem IrOp nova: IR_REDIMENSIONA (op_aux REDIM_CAP / REDIM_RESERVA para cap e reserve), IR_ANEXA, IR_ELEMENTO,
 * IR_ENDERECO_ELEMENTO + IR_GRAVA_PONTEIRO, IR_PARA, IR_SE, IR_EMBRULHA. Os erros de execucao saem das checagens de sempre (dev): indice fora = R0006 em
 * ELEMENTO / ENDERECO_ELEMENTO com op_aux 0; release nao confere nada (como l[i]).
 *   clear        resize(&l, len = 0).                       reserve  IR_REDIMENSIONA REDIM_RESERVA.        cap  IR_REDIMENSIONA REDIM_CAP.
 *   truncate     n != len: le l[n] (R0006 em dev se n < 0 ou n > len); depois encolhe para n.
 *   pop          none se len == 0; senao l[len-1] embrulhado e resize(len-1).
 *   insert_at    append(item) (cresce 1); laco que leva l[j-1] para l[j] de len ate i + 1; l[i] = item (R0006 em dev se i < 0 ou i > len).
 *   ordered_remove    le l[i] (R0006 em dev), laco que leva l[j+1] para l[j] de i ate len - 2, resize(len - 1).
 *   unordered_remove  l[i] = l[len-1] (R0006 em dev se i fora) e resize(len - 1).
 *   remove_value      acha a 1a posicao (laco com PARE) e faz o ordered_remove nela; devolve se achou.
 *   dedup             w = 1; laco de 1 a n: se l[r] != l[w - 1], l[w] = l[r] e w + 1; depois resize(&l, len = w). Lista vazia: nada (SE na guarda).
 * Versao: 0.6.0
 * Changelog: 0.6.0 - K15: LwSetUnion, LwSetIntersect e LwSetDiff (IR_PARA sobre o map, IR_MAPA_ITEM op_aux 0, IR_MAPA_TEM, IR_MAPA_INSERE, IR_MAPA_REMOVE; sem IrOp nova); prototipos em lower_call.c.
 *            0.5.0 - K09: LwIndexBy, LwFindBy, LwAnyBy, LwAllBy e LwCountBy (laco IR_PARA sobre a fatia, IR_CHAMADA_IND do pred com &s[i] por LlEnd, IR_SE e PARE; sem IrOp nova); prototipos em lower_call.c.
 *            0.4.0 - K17: LwHeapPush, LwHeapPushBy, LwHeapPop, LwHeapPopBy (IR_ANEXA + IR_HEAP; IR_ELEMENTO e IR_REDIMENSIONA como o pop); prototipos em lower_call.c.
 *            0.3.0 - K08c: LwDedup (EMB_DEDUP, sem IrOp nova: LoLocal, IR_PARA, IR_ELEMENTO, IR_GRAVA_PONTEIRO, IR_REDIMENSIONA); prototipo em lower_call.c.
 *            0.2.0 - IA1-L9e: clear(&m) de map desvia para LwClearMapa (lower_mapa_vistas.c).
 *            0.1.0 - IA1-L9c/L9d: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_lista.h"

/* ---------- pecas ---------- */

const Expr *LlArgumento(const Expr *e, int32_t indice)
{
    for (uint32_t k = 0; k < e->nargs; k++) {
        if (e->args[k].indice == indice) return e->args[k].valor;
    }
    return NULL;
}

LlBase LlBaseDe(const LowerCtx *c, Valor valor)
{
    const Tipos *tipos = &c->sem->tipos;
    if (TipoClasseDe(tipos, valor.tipo) == TC_FATIA) return (LlBase){valor, (TipoId)TipoDe(tipos, valor.tipo)->a, false};
    TipoId lista = (TipoId)TipoDe(tipos, valor.tipo)->a; /* ^[dynamic]T -> [dynamic]T */
    return (LlBase){valor, (TipoId)TipoDe(tipos, lista)->a, true};
}

Valor LlIndiceInt(const LoCtx *k, const Expr *e)
{
    return LoConverte(k, TIPO_INT, LwExpr(k->c, e));
}

static Valor TempDe(const LoCtx *k, IrOp op, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(k->c, op, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = tipo;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, tipo);
}

Valor LlTamanho(const LoCtx *k, Valor colecao)
{
    return TempDe(k, IR_TAMANHO_TEXTO, TIPO_INT, colecao, LwValorNenhum());
}

/* Em fatia o indice provado dispensa a checagem; em lista o op_aux 0 mantem a checagem da epoca (R0008) do frame. */
Valor LlLe(const LoCtx *k, const LlBase *b, Valor indice, bool provado)
{
    IrInstr *i = LwNovaInstr(k->c, IR_ELEMENTO, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = b->elemento;
    i->a = b->valor;
    i->b = indice;
    i->op_aux = provado && !b->lista ? 1 : 0;
    return LwValorTemp(i->destino, b->elemento);
}

Valor LlEnd(const LoCtx *k, const LlBase *b, Valor indice, bool provado)
{
    TipoId ponteiro = TipoPonteiro(&k->c->sem->tipos, b->elemento);
    IrInstr *i = LwNovaInstr(k->c, IR_ENDERECO_ELEMENTO, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = ponteiro;
    i->a = b->valor;
    i->b = indice;
    i->op_aux = provado && !b->lista ? 1 : 0;
    return LwValorTemp(i->destino, ponteiro);
}

void LlGrava(const LoCtx *k, Valor endereco, TipoId elemento, Valor valor)
{
    IrInstr *i = LwNovaInstr(k->c, IR_GRAVA_PONTEIRO, k->pos);
    i->tipo = elemento;
    i->a = endereco;
    i->b = valor;
    i->nome = -1;
}

Valor LlEmbrulha(const LoCtx *k, TipoId opcional, Valor valor)
{
    return TempDe(k, IR_EMBRULHA, opcional, valor, LwValorNenhum());
}

Valor LlEntraLaco(const LoCtx *k, Valor inicio, Valor fim, IrBloco **externo)
{
    IrInstr *laco = LwNovaInstr(k->c, IR_PARA, k->pos);
    int32_t contador = LwNovoLocal(k->c);
    laco->destino = contador;
    laco->tipo = TIPO_INT;
    laco->a = inicio;
    laco->b = fim;
    laco->faixa_excl = true;
    *externo = LwTrocarBloco(k->c, &laco->corpo);
    return LwValorLocal(contador, TIPO_INT);
}

void LlSaiDoLaco(const LoCtx *k, IrBloco *externo)
{
    LwTrocarBloco(k->c, externo);
}

void LlPare(const LoCtx *k)
{
    LwNovaInstr(k->c, IR_PARE, k->pos);
}

void LlRedimensiona(const LoCtx *k, Valor lista, TipoId elemento, Valor novo_len)
{
    IrInstr *i = LwNovaInstr(k->c, IR_REDIMENSIONA, k->pos);
    i->tipo = elemento;
    i->a = lista;
    i->b = novo_len;
}

Valor LlMais(const LoCtx *k, Valor a, Valor b)
{
    return LoBinario(k, OP_SOMA, TIPO_INT, a, b);
}

Valor LlMenos(const LoCtx *k, Valor a, Valor b)
{
    return LoBinario(k, OP_SUB, TIPO_INT, a, b);
}

Valor LlIgual(const LoCtx *k, Valor a, Valor b)
{
    return LoBinario(k, OP_IGUAL, TIPO_BOOL, a, b);
}

/* ---------- os embutidos ---------- */

#define INT_VALOR(n) LwValorInt((n), TIPO_INT)

/* cap(l): l e [dynamic]T ou ^[dynamic]T. */
static Valor LwCap(LowerCtx *c, const Expr *e)
{
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    IrInstr *i = LwNovaInstr(c, IR_REDIMENSIONA, e->pos_op);
    i->op_aux = REDIM_CAP;
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_INT;
    i->a = lista;
    return LwValorTemp(i->destino, TIPO_INT);
}

static Valor LwClear(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    TipoId alvo = LlArgumento(e, 0)->tipo;
    if (TipoClasseDe(&c->sem->tipos, TipoDe(&c->sem->tipos, alvo)->a) == TC_MAPA) return LwClearMapa(c, e); /* clear(&m) de map (L9e); alvo e ^map ou ^[dynamic]T */
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    LlRedimensiona(&k, lista, LlBaseDe(c, lista).elemento, INT_VALOR(0));
    return LwValorNenhum();
}

static Valor LwReserve(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor n = LlIndiceInt(&k, LlArgumento(e, 1));
    IrInstr *i = LwNovaInstr(c, IR_REDIMENSIONA, e->pos_op);
    i->op_aux = REDIM_RESERVA;
    i->tipo = LlBaseDe(c, lista).elemento;
    i->a = lista;
    i->b = n;
    return LwValorNenhum();
}

/* truncate(&l, len = n): n != len le l[n] (checado: R0006 em dev se n < 0 ou n > len); depois so encolhe. */
static Valor LwTruncate(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor n = LlIndiceInt(&k, LlArgumento(e, 1));
    LlBase l = LlBaseDe(c, lista);
    IrBloco *fora = LoEntraSe(&k, LoBinario(&k, OP_DIFERENTE, TIPO_BOOL, n, LlTamanho(&k, lista)));
    LlLe(&k, &l, n, false);
    LoSaiDoSe(&k, fora);
    LlRedimensiona(&k, lista, l.elemento, n);
    return LwValorNenhum();
}

/* pop(&l) -> ?T: none com len 0; senao o ultimo, embrulhado, e a lista encolhe um. */
static Valor LwPop(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    LlBase l = LlBaseDe(c, lista);
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    Valor n = LlTamanho(&k, lista);
    IrBloco *fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, n, INT_VALOR(0)));
    Valor ultimo = LlMenos(&k, n, INT_VALOR(1));
    LoAtribui(&k, resultado, e->tipo, LlEmbrulha(&k, e->tipo, LlLe(&k, &l, ultimo, true)));
    LlRedimensiona(&k, lista, l.elemento, ultimo);
    LoSaiDoSe(&k, fora);
    return LwValorLocal(resultado, e->tipo);
}

/* insert_at(&l, i = k, item = x): ver o cabecalho. O item e avaliado antes de a lista mudar. */
static Valor LwInsertAt(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo;
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor indice = LlIndiceInt(&k, LlArgumento(e, 1));
    Valor item = LwExpr(c, LlArgumento(e, 2));
    LlBase l = LlBaseDe(c, lista);
    Valor n = LlTamanho(&k, lista);
    IrInstr *anexa = LwNovaInstr(c, IR_ANEXA, e->pos_op);
    anexa->tipo = l.elemento;
    anexa->a = lista;
    anexa->b = item;
    Valor passos = LlMenos(&k, n, indice);
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), passos, &externo);
    Valor j = LlMenos(&k, n, volta);
    LlGrava(&k, LlEnd(&k, &l, j, true), l.elemento, LlLe(&k, &l, LlMenos(&k, j, INT_VALOR(1)), false));
    LlSaiDoLaco(&k, externo);
    LlGrava(&k, LlEnd(&k, &l, indice, false), l.elemento, item);
    return LwValorNenhum();
}

/* Tira o elemento i (ja conferido) fechando o buraco: l[j] = l[j + 1] de i ate n - 2, depois len = n - 1. */
static void RemoveFechando(const LoCtx *k, const LlBase *l, Valor n, Valor i)
{
    IrBloco *externo;
    Valor ultimo = LlMenos(k, n, INT_VALOR(1));
    Valor volta = LlEntraLaco(k, INT_VALOR(0), LlMenos(k, ultimo, i), &externo);
    Valor j = LlMais(k, i, volta);
    LlGrava(k, LlEnd(k, l, j, true), l->elemento, LlLe(k, l, LlMais(k, j, INT_VALOR(1)), true));
    LlSaiDoLaco(k, externo);
    LlRedimensiona(k, l->valor, l->elemento, ultimo);
}

static Valor LwOrderedRemove(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor indice = LlIndiceInt(&k, LlArgumento(e, 1));
    LlBase l = LlBaseDe(c, lista);
    LlLe(&k, &l, indice, false); /* so a checagem de indice (R0006 em dev) */
    RemoveFechando(&k, &l, LlTamanho(&k, lista), indice);
    return LwValorNenhum();
}

/* unordered_remove: o ultimo vai para o lugar do tirado (se i e o ultimo, grava nele mesmo) e a lista encolhe um. */
static Valor LwUnorderedRemove(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor indice = LlIndiceInt(&k, LlArgumento(e, 1));
    LlBase l = LlBaseDe(c, lista);
    Valor alvo = LlEnd(&k, &l, indice, false);
    Valor ultimo = LlMenos(&k, LlTamanho(&k, lista), INT_VALOR(1));
    LlGrava(&k, alvo, l.elemento, LlLe(&k, &l, ultimo, true));
    LlRedimensiona(&k, lista, l.elemento, ultimo);
    return LwValorNenhum();
}

/* remove_value(&l, value = v) -> bool: a 1a posicao igual a v sai com o ordered_remove. */
static Valor LwRemoveValue(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo, *fora;
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor valor = LwExpr(c, LlArgumento(e, 1));
    LlBase l = LlBaseDe(c, lista);
    Valor n = LlTamanho(&k, lista);
    int32_t achou = LoLocal(&k, TIPO_INT, INT_VALOR(-1));
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    fora = LoEntraSe(&k, LlIgual(&k, LlLe(&k, &l, volta, true), valor));
    LoAtribui(&k, achou, TIPO_INT, volta);
    LlPare(&k);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    int32_t resultado = LoLocal(&k, TIPO_BOOL, LwValorBool(false));
    Valor posicao = LwValorLocal(achou, TIPO_INT);
    fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR_IGUAL, TIPO_BOOL, posicao, INT_VALOR(0)));
    RemoveFechando(&k, &l, n, posicao);
    LoAtribui(&k, resultado, TIPO_BOOL, LwValorBool(true));
    LoSaiDoSe(&k, fora);
    return LwValorLocal(resultado, TIPO_BOOL);
}

/* dedup: compara com o ultimo que ficou (l[w - 1]), como slices.Compact; real NaN != NaN, entao nao se funde.
 * Laco de 1 a n: l[w] recebe l[r] so quando e diferente; w sai do laco com o novo len. */
static void DedupCompacta(const LoCtx *k, const LlBase *l, Valor n, int32_t w)
{
    IrBloco *externo, *fora;
    Valor volta = LlEntraLaco(k, INT_VALOR(1), n, &externo);
    Valor atual = LlLe(k, l, volta, true);
    Valor anterior = LlLe(k, l, LlMenos(k, LwValorLocal(w, TIPO_INT), INT_VALOR(1)), true);
    fora = LoEntraSe(k, LoBinario(k, OP_DIFERENTE, TIPO_BOOL, atual, anterior));
    LlGrava(k, LlEnd(k, l, LwValorLocal(w, TIPO_INT), true), l->elemento, atual);
    LoAtribui(k, w, TIPO_INT, LlMais(k, LwValorLocal(w, TIPO_INT), INT_VALOR(1)));
    LoSaiDoSe(k, fora);
    LlSaiDoLaco(k, externo);
}

/* dedup(&l): lista vazia nao muda (a guarda n > 0 sai antes de criar a escrita); senao compacta e encolhe para w. */
Valor LwDedup(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    LlBase l = LlBaseDe(c, lista);
    Valor n = LlTamanho(&k, lista);
    int32_t w = LoLocal(&k, TIPO_INT, INT_VALOR(1));
    IrBloco *fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, n, INT_VALOR(0)));
    DedupCompacta(&k, &l, n, w);
    LlRedimensiona(&k, lista, l.elemento, LwValorLocal(w, TIPO_INT));
    LoSaiDoSe(&k, fora);
    return LwValorNenhum();
}

/* K09: fatia (arg 0) e pred (arg 1) saem antes do laco (LwExpr pode emitir); devolve a base da fatia, o pred e o len em *n. */
static LlBase PredParametros(LowerCtx *c, const Expr *e, Valor *pred, Valor *n)
{
    LoCtx k = {c, e->pos_op};
    Valor fatia = LwExpr(c, LlArgumento(e, 0));
    *pred = LwExpr(c, LlArgumento(e, 1));
    *n = LlTamanho(&k, fatia);
    return LlBaseDe(c, fatia);
}

/* K09: pred(&s[i]) como IR_CHAMADA_IND sobre o proc ja avaliado; o bool sai num temporario. */
static Valor PredicadoEm(const LoCtx *k, Valor pred, const LlBase *b, Valor volta)
{
    Valor endereco = LlEnd(k, b, volta, true); /* antes da chamada: o endereco e emitido na frente dela */
    IrInstr *i = LwNovaInstr(k->c, IR_CHAMADA_IND, k->pos);
    i->a = pred;
    i->tipo = TIPO_BOOL;
    i->args = LwArgs(k->c, 1);
    i->args[0] = endereco;
    i->nargs = 1;
    i->destino = LwNovoTemp(k->c);
    return LwValorTemp(i->destino, TIPO_BOOL);
}

/* index_by(s, pred = p) -> (int, bool) (K09): o 1o i que satisfaz p, com PARE no achado; nenhum = (-1, false). */
Valor LwIndexBy(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor pred, n;
    LlBase b = PredParametros(c, e, &pred, &n);
    int32_t posicao = LoLocal(&k, TIPO_INT, INT_VALOR(-1));
    int32_t achou = LoLocal(&k, TIPO_BOOL, LwValorNenhum());
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    IrBloco *fora = LoEntraSe(&k, PredicadoEm(&k, pred, &b, volta));
    LoAtribui(&k, posicao, TIPO_INT, volta);
    LoAtribui(&k, achou, TIPO_BOOL, LwValorBool(true));
    LlPare(&k);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    IrInstr *tupla = LwNovaInstr(c, IR_TUPLA, e->pos_op);
    tupla->destino = LwNovoTemp(c);
    tupla->tipo = e->tipo;
    tupla->args = LwArgs(c, 2);
    tupla->args[0] = LwValorLocal(posicao, TIPO_INT);
    tupla->args[1] = LwValorLocal(achou, TIPO_BOOL);
    tupla->nargs = 2;
    return LwValorTemp(tupla->destino, e->tipo);
}

/* find_by(s, pred = p) -> ?T (K09): o 1o elemento que satisfaz p, embrulhado; nenhum = none. */
Valor LwFindBy(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor pred, n;
    LlBase b = PredParametros(c, e, &pred, &n);
    int32_t achado = LoLocal(&k, e->tipo, LwValorNenhum());
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    IrBloco *fora = LoEntraSe(&k, PredicadoEm(&k, pred, &b, volta));
    LoAtribui(&k, achado, e->tipo, LlEmbrulha(&k, e->tipo, LlLe(&k, &b, volta, true)));
    LlPare(&k);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(achado, e->tipo);
}

/* any_by (todos = false) e all_by (todos = true) -> bool (K09): any para no 1o verdadeiro (false se nenhum); all para no 1o falso (true se nenhum). */
static Valor LwAnyAll(LowerCtx *c, const Expr *e, bool todos)
{
    LoCtx k = {c, e->pos_op};
    Valor pred, n;
    LlBase b = PredParametros(c, e, &pred, &n);
    int32_t resultado = LoLocal(&k, TIPO_BOOL, LwValorBool(todos));
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    Valor cond = PredicadoEm(&k, pred, &b, volta);
    IrBloco *fora = LoEntraSe(&k, todos ? LoNao(&k, cond) : cond);
    LoAtribui(&k, resultado, TIPO_BOOL, LwValorBool(!todos));
    LlPare(&k);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(resultado, TIPO_BOOL);
}

Valor LwAnyBy(LowerCtx *c, const Expr *e) { return LwAnyAll(c, e, false); }
Valor LwAllBy(LowerCtx *c, const Expr *e) { return LwAnyAll(c, e, true); }

/* count_by(s, pred = p) -> int (K09): quantos elementos satisfazem p; sem PARE (conta todos). */
Valor LwCountBy(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor pred, n;
    LlBase b = PredParametros(c, e, &pred, &n);
    int32_t quantos = LoLocal(&k, TIPO_INT, INT_VALOR(0));
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    IrBloco *fora = LoEntraSe(&k, PredicadoEm(&k, pred, &b, volta));
    LoAtribui(&k, quantos, TIPO_INT, LlMais(&k, LwValorLocal(quantos, TIPO_INT), INT_VALOR(1)));
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(quantos, TIPO_INT);
}

/* K15: o map (ou ^map) como base do IR: ^map pelo valor; map enderecavel pelo endereco (a regra do BaseDoMapa de lower_mapa_vistas.c). */
static Valor ConjuntoBase(LowerCtx *c, const Expr *arg)
{
    if (TipoClasseDe(&c->sem->tipos, arg->tipo) == TC_PONTEIRO) return LwExpr(c, arg);
    return LwEnderecavel(c, arg) ? LwEndereco(c, arg) : LwExpr(c, arg);
}

/* K15: K do map[K]bool de um map ou ^map. */
static TipoId ChaveDoConjunto(const LowerCtx *c, TipoId t)
{
    TipoId m = TipoClasseDe(&c->sem->tipos, t) == TC_PONTEIRO ? (TipoId)TipoDe(&c->sem->tipos, t)->a : t;
    return (TipoId)TipoDe(&c->sem->tipos, m)->a;
}

/* K15: IR_MAPA_ITEM op_aux 0 = a chave do par denso `indice` do map. */
static Valor ConjuntoChaveDo(const LoCtx *k, Valor mapa, Valor indice, TipoId chave)
{
    IrInstr *i = LwNovaInstr(k->c, IR_MAPA_ITEM, k->pos);
    i->op_aux = 0;
    i->tipo = chave;
    i->a = mapa;
    i->b = indice;
    i->destino = LwNovoTemp(k->c);
    return LwValorTemp(i->destino, chave);
}

/* K15: IR_MAPA_TEM: a chave esta no map (o membro do conjunto). */
static Valor ConjuntoTem(const LoCtx *k, Valor mapa, Valor chave)
{
    IrInstr *i = LwNovaInstr(k->c, IR_MAPA_TEM, k->pos);
    i->tipo = TIPO_BOOL;
    i->a = mapa;
    i->b = chave;
    i->destino = LwNovoTemp(k->c);
    return LwValorTemp(i->destino, TIPO_BOOL);
}

/* K15: IR_MAPA_INSERE (chave com true) e IR_MAPA_REMOVE (chave sai) sobre o ^map. */
static void ConjuntoInsere(const LoCtx *k, Valor mapa, Valor chave)
{
    IrInstr *i = LwNovaInstr(k->c, IR_MAPA_INSERE, k->pos);
    i->tipo = TIPO_BOOL;
    i->a = mapa;
    i->b = chave;
    i->args = LwArgs(k->c, 1);
    i->args[0] = LwValorBool(true);
    i->nargs = 1;
}

static void ConjuntoRemove(const LoCtx *k, Valor mapa, Valor chave)
{
    IrInstr *i = LwNovaInstr(k->c, IR_MAPA_REMOVE, k->pos);
    i->tipo = TIPO_BOOL;
    i->a = mapa;
    i->b = chave;
}

/* set_union(&a, b) (K15): cada chave de b entra em a com true; a presenca da chave e o membro. */
Valor LwSetUnion(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor a = LwExpr(c, LlArgumento(e, 0));
    Valor b = ConjuntoBase(c, LlArgumento(e, 1));
    TipoId chave = ChaveDoConjunto(c, LlArgumento(e, 0)->tipo);
    Valor n = LlTamanho(&k, b);
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    ConjuntoInsere(&k, a, ConjuntoChaveDo(&k, b, volta, chave));
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}

/* set_intersect e set_diff(&a, b) (K15): de tras para frente sobre a (indice len - 1 - volta); a chave sai de a quando nao esta em b (intersect) ou quando esta (diff).
 * Como em clear, o remove so mexe no par do proprio indice e nos de cima, ja vistos; por isso nao ha pulo de par. */
static Valor ConjuntoEnxuga(LowerCtx *c, const Expr *e, bool intersecao)
{
    LoCtx k = {c, e->pos_op};
    Valor a = LwExpr(c, LlArgumento(e, 0));
    Valor b = ConjuntoBase(c, LlArgumento(e, 1));
    TipoId chave = ChaveDoConjunto(c, LlArgumento(e, 0)->tipo);
    Valor n = LlTamanho(&k, a);
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    Valor cha = ConjuntoChaveDo(&k, a, LlMenos(&k, LlMenos(&k, n, INT_VALOR(1)), volta), chave);
    Valor em_b = ConjuntoTem(&k, b, cha);
    IrBloco *fora = LoEntraSe(&k, intersecao ? LoNao(&k, em_b) : em_b);
    ConjuntoRemove(&k, a, cha);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}

Valor LwSetIntersect(LowerCtx *c, const Expr *e) { return ConjuntoEnxuga(c, e, true); }
Valor LwSetDiff(LowerCtx *c, const Expr *e) { return ConjuntoEnxuga(c, e, false); }

/* heap (K17): o IR_HEAP leva a lista (a), o tipo T (tipo) e a ordem: b = proc less so nos _BY. tipo_chave = ORDENA_* de T, so sem less (com less o runtime
 * nao le o tipo e o elemento pode ser enum ou struct, que TipoDeOrdenacao nao aceita). */
static void HeapInstrucao(LowerCtx *c, const Expr *e, Valor lista, TipoId elemento, uint8_t modo, Valor comparador)
{
    IrInstr *i = LwNovaInstr(c, IR_HEAP, e->pos_op);
    bool com_less = modo == HEAP_SUBIR_BY || modo == HEAP_RETIRA_TOPO_BY;
    i->tipo = elemento;
    i->a = lista;
    i->b = comparador;
    i->op_aux = modo;
    i->tipo_chave = com_less ? 0 : TipoDeOrdenacao(c, elemento);
    c->prog->usa_heap = true;
    c->prog->usa_ordenar_chave = true; /* o C emitido do heap usa forja_ordc_menor (forja_rt_ordenar_chave.h) */
}

/* heap_push(&l, v) e heap_push_by(&l, v, less = p) (K17): o item entra pelo append (IR_ANEXA, como insert_at) e o IR_HEAP sobe o ultimo. */
static Valor LwHeapEmpurra(LowerCtx *c, const Expr *e, bool com_less)
{
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor item = LwExpr(c, LlArgumento(e, 1));
    Valor comparador = com_less ? LwExpr(c, LlArgumento(e, 2)) : LwValorNenhum();
    LlBase l = LlBaseDe(c, lista);
    IrInstr *anexa = LwNovaInstr(c, IR_ANEXA, e->pos_op);
    anexa->tipo = l.elemento;
    anexa->a = lista;
    anexa->b = item;
    HeapInstrucao(c, e, lista, l.elemento, com_less ? HEAP_SUBIR_BY : HEAP_SUBIR, comparador);
    return LwValorNenhum();
}

/* heap_pop(&l) e heap_pop_by(&l, less = p) -> ?T (K17): lista vazia = none; senao o IR_HEAP leva o topo para l[len - 1], e aqui se le, embrulha e encolhe um. */
static Valor LwHeapTira(LowerCtx *c, const Expr *e, bool com_less)
{
    LoCtx k = {c, e->pos_op};
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    Valor comparador = com_less ? LwExpr(c, LlArgumento(e, 1)) : LwValorNenhum();
    LlBase l = LlBaseDe(c, lista);
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    Valor n = LlTamanho(&k, lista);
    IrBloco *fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, n, INT_VALOR(0)));
    Valor ultimo = LlMenos(&k, n, INT_VALOR(1));
    HeapInstrucao(c, e, lista, l.elemento, com_less ? HEAP_RETIRA_TOPO_BY : HEAP_RETIRA_TOPO, comparador);
    LoAtribui(&k, resultado, e->tipo, LlEmbrulha(&k, e->tipo, LlLe(&k, &l, ultimo, true)));
    LlRedimensiona(&k, lista, l.elemento, ultimo);
    LoSaiDoSe(&k, fora);
    return LwValorLocal(resultado, e->tipo);
}

/* Tratadores exatos (lower_call.c): heap_push, heap_push_by, heap_pop e heap_pop_by. */
Valor LwHeapPush(LowerCtx *c, const Expr *e) { return LwHeapEmpurra(c, e, false); }
Valor LwHeapPushBy(LowerCtx *c, const Expr *e) { return LwHeapEmpurra(c, e, true); }
Valor LwHeapPop(LowerCtx *c, const Expr *e) { return LwHeapTira(c, e, false); }
Valor LwHeapPopBy(LowerCtx *c, const Expr *e) { return LwHeapTira(c, e, true); }

Valor LwLista(LowerCtx *c, const Expr *e)
{
    switch (e->ref_id) {
    case EMB_CAP: return LwCap(c, e);
    case EMB_CLEAR: return LwClear(c, e);
    case EMB_RESERVE: return LwReserve(c, e);
    case EMB_TRUNCATE: return LwTruncate(c, e);
    case EMB_POP: return LwPop(c, e);
    case EMB_INSERT_AT: return LwInsertAt(c, e);
    case EMB_ORDERED_REMOVE: return LwOrderedRemove(c, e);
    case EMB_UNORDERED_REMOVE: return LwUnorderedRemove(c, e);
    case EMB_REMOVE_VALUE: return LwRemoveValue(c, e);
    default: return LwBusca(c, e);
    }
}
