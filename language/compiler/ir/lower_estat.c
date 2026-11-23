/* lower_estat.c - lowering da estatistica de fatia (IA1-L9e, K07): min_of, max_of, sum, avg; e a porta dos 8 embutidos novos da L9e (LwEstat). A semantica ja trocou
 * lista e array por s[..], entao o 1o argumento chega como fatia []T. Sem IrOp nova: IR_PARA com IR_ELEMENTO (indice provado), IR_BINARIO, IR_CONVERSAO e IR_SE.
 *   min_of / max_of   resultado ?T nasce none; com len > 0, o melhor comeca em s[0] e o laco 1..<len o troca por x < melhor (x > melhor); em real tambem troca quando o
 *                     melhor e NaN (melhor != melhor): NaN e ignorado e so sobra se todos forem NaN. O melhor sai embrulhado.
 *   sum               acumulador T (0 ou 0.0) mais cada elemento por IR_BINARIO OP_SOMA em T: R0002 em dev no inteiro que estoura, volta em release (como a + b); real propaga NaN.
 *   avg               acumulador f64 mais cada elemento convertido para f64; com len > 0, resultado = soma / len; len 0 = 0.0.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-L9f: EMB_TO_STR despacha para LwToStr (lower_texto_builder.c).
 *            0.1.0 - IA1-L9e: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_lista.h"

#define INT_VALOR(n) LwValorInt((n), TIPO_INT)

/* A fatia do argumento 0, a base dela e o len. */
typedef struct { LlBase base; Valor n; } Fatia;

static Fatia FatiaDoArgumento(const LoCtx *k, const Expr *arg)
{
    Valor fatia = LwExpr(k->c, arg);
    return (Fatia){LlBaseDe(k->c, fatia), LlTamanho(k, fatia)};
}

static Valor ZeroDe(const LoCtx *k, TipoId tipo)
{
    return TipoEhReal(&k->c->sem->tipos, tipo) ? LwValorReal(0.0, tipo) : LwValorInt(0, tipo);
}

/* "x vence o melhor": x < melhor (min) ou x > melhor (max); em real, tambem quando o melhor e NaN. */
static Valor Vence(const LoCtx *k, bool minimo, TipoId tipo, Valor x, Valor melhor)
{
    Valor vence = LoBinario(k, minimo ? OP_MENOR : OP_MAIOR, TIPO_BOOL, x, melhor);
    if (!TipoEhReal(&k->c->sem->tipos, tipo)) return vence;
    return LoBinario(k, OP_OU, TIPO_BOOL, vence, LoBinario(k, OP_DIFERENTE, TIPO_BOOL, melhor, melhor));
}

/* min_of(s) e max_of(s) -> ?T. */
static Valor LwMinMax(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo, *fora, *troca;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    TipoId t = f.base.elemento;
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, f.n, INT_VALOR(0)));
    int32_t melhor = LoLocal(&k, t, LlLe(&k, &f.base, INT_VALOR(0), true));
    Valor volta = LlEntraLaco(&k, INT_VALOR(1), f.n, &externo);
    Valor x = LlLe(&k, &f.base, volta, true);
    troca = LoEntraSe(&k, Vence(&k, e->ref_id == EMB_MIN_OF, t, x, LwValorLocal(melhor, t)));
    LoAtribui(&k, melhor, t, x);
    LoSaiDoSe(&k, troca);
    LlSaiDoLaco(&k, externo);
    LoAtribui(&k, resultado, e->tipo, LlEmbrulha(&k, e->tipo, LwValorLocal(melhor, t)));
    LoSaiDoSe(&k, fora);
    return LwValorLocal(resultado, e->tipo);
}

/* sum(s) -> T. */
static Valor LwSum(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    int32_t soma = LoLocal(&k, e->tipo, ZeroDe(&k, e->tipo));
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), f.n, &externo);
    Valor atual = LoBinario(&k, OP_SOMA, e->tipo, LwValorLocal(soma, e->tipo), LlLe(&k, &f.base, volta, true));
    LoAtribui(&k, soma, e->tipo, atual);
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(soma, e->tipo);
}

/* avg(s) -> f64: soma em f64 dividida pelo len; vazia = 0.0. */
static Valor LwAvg(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo, *fora;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    int32_t media = LoLocal(&k, TIPO_F64, LwValorReal(0.0, TIPO_F64));
    fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, f.n, INT_VALOR(0)));
    int32_t soma = LoLocal(&k, TIPO_F64, LwValorReal(0.0, TIPO_F64));
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), f.n, &externo);
    Valor x = LoConverte(&k, TIPO_F64, LlLe(&k, &f.base, volta, true));
    LoAtribui(&k, soma, TIPO_F64, LoBinario(&k, OP_SOMA, TIPO_F64, LwValorLocal(soma, TIPO_F64), x));
    LlSaiDoLaco(&k, externo);
    LoAtribui(&k, media, TIPO_F64, LoBinario(&k, OP_DIV, TIPO_F64, LwValorLocal(soma, TIPO_F64), LoConverte(&k, TIPO_F64, f.n)));
    LoSaiDoSe(&k, fora);
    return LwValorLocal(media, TIPO_F64);
}

Valor LwEstat(LowerCtx *c, const Expr *e)
{
    switch (e->ref_id) {
    case EMB_MIN_OF:
    case EMB_MAX_OF: return LwMinMax(c, e);
    case EMB_SUM: return LwSum(c, e);
    case EMB_AVG: return LwAvg(c, e);
    case EMB_KEYS:
    case EMB_VALUES: return LwVistasDoMapa(c, e);
    case EMB_FROM_BYTES: return LwTextoCopia(c, e);
    case EMB_AS_BYTES: return LwAsBytes(c, e);
    case EMB_TO_STR: return LwToStr(c, e);
    default: FalhaInterna("lowering: embutido de estatistica desconhecido (offset %u)", (unsigned)e->pos_op);
    }
}
