/* lower_sorteio.c - abaixamento de shuffle(s, rng = &r) e choice(s, rng = &r) (S2b). Sem IrOp nova: laco IR_PARA com IR_ELEMENTO/IR_ENDERECO_ELEMENTO
 * (como o swap de lower_busca.c) e IR_CHAMADA direta ao proc below de std:rand.
 *   shuffle  para i em 0..<n: j = below(rng, i + 1) como int; troca s[i] com s[j]. E o laco do perm de std:rand, com os mesmos sorteios na mesma ordem.
 *   choice   n == 0 -> none SEM chamar below (o gerador nao avanca); senao o elemento s[below(rng, n)] embrulhado em ?T.
 * O proc below sai do modulo do tipo do rng (SemBelowDoRng, sem_sorteio.c), nunca do nome solto. A semantica ja trocou a colecao por s[..].
 * Versao: 0.1.0
 * Changelog: 0.1.0 - S2b: primeira versao (LwShuffle e LwChoice; prototipos locais em lower_call.c).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py --forja compiler\forja.exe
 */
#include "ir/lower_lista.h"

#include "semantic/sem_lista.h"

#define INT_VALOR(n) LwValorInt((n), TIPO_INT)

/* O proc below do rng (o Valor de tipo ^Rng). Ausente aqui = bug: a semantica ja deu E0338 para o que nao e ^rand.Rng. */
static const Decl *BelowDoRng(const LoCtx *k, Valor rng)
{
    const Decl *below = SemBelowDoRng(k->c->sem, rng.tipo);
    if (below == NULL) FalhaInterna("lowering: rng de shuffle/choice sem o below de std:rand (offset %u)", (unsigned)k->pos);
    return below;
}

/* IR_CHAMADA a below(rng, n): n ja e u64; devolve o u64 sorteado em 0..<n (o proc de std:rand, chave por modulo). */
static Valor SorteioBelow(const LoCtx *k, Valor rng, const Decl *below, Valor n)
{
    LowerCtx *c = k->c;
    Valor *args = LwArgs(c, 2);
    IrInstr *i = LwNovaInstr(c, IR_CHAMADA, k->pos);
    args[0] = rng;
    args[1] = n;
    i->nome = below->chave;
    i->tipo = TIPO_UINT;
    i->args = args;
    i->nargs = 2;
    i->destino = LwNovoTemp(c);
    return LwValorTemp(i->destino, TIPO_UINT);
}

/* Um passo do shuffle: j = below(rng, i + 1) como int; le s[i] e s[j] antes de gravar, para a troca nao perder um dos dois. */
static void TrocaComSorteado(const LoCtx *k, const LlBase *b, Valor rng, const Decl *below, Valor i)
{
    Valor n = LoConverte(k, TIPO_UINT, LlMais(k, i, INT_VALOR(1)));
    Valor j = LoConverte(k, TIPO_INT, SorteioBelow(k, rng, below, n));
    Valor em_i = LlLe(k, b, i, true);
    Valor em_j = LlLe(k, b, j, false);
    LlGrava(k, LlEnd(k, b, i, true), b->elemento, em_j);
    LlGrava(k, LlEnd(k, b, j, false), b->elemento, em_i);
}

/* shuffle(s, rng = &r): laco 0..<len(s) com a troca de cada passo; sem valor. */
Valor LwShuffle(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor colecao = LwExpr(c, LlArgumento(e, 0));
    LlBase b = LlBaseDe(c, colecao);
    Valor rng = LwExpr(c, LlArgumento(e, 1));
    const Decl *below = BelowDoRng(&k, rng);
    Valor n = LlTamanho(&k, colecao);
    IrBloco *externo;
    Valor i = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    TrocaComSorteado(&k, &b, rng, below, i);
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}

/* choice(s, rng = &r) -> ?T: none se a colecao esta vazia (sem sorteio); senao o s[below(rng, n)] embrulhado. */
Valor LwChoice(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor colecao = LwExpr(c, LlArgumento(e, 0));
    LlBase b = LlBaseDe(c, colecao);
    Valor rng = LwExpr(c, LlArgumento(e, 1));
    const Decl *below = BelowDoRng(&k, rng);
    Valor n = LlTamanho(&k, colecao);
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    IrBloco *fora = LoEntraSe(&k, LoNao(&k, LlIgual(&k, n, INT_VALOR(0))));
    Valor j = LoConverte(&k, TIPO_INT, SorteioBelow(&k, rng, below, LoConverte(&k, TIPO_UINT, n)));
    LoAtribui(&k, resultado, e->tipo, LlEmbrulha(&k, e->tipo, LlLe(&k, &b, j, false)));
    LoSaiDoSe(&k, fora);
    return LwValorLocal(resultado, e->tipo);
}
