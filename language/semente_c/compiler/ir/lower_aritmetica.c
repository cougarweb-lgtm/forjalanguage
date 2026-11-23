/* lower_aritmetica.c - lowering de wrapping_*, saturating_*, checked_*, overflowing_* e mul_hi_u64 (IA1-L9a, IA1-L9b). Sem IrOp nova:
 *   wrapping_add/sub/mul = IR_BINARIO com OP_SOMA_VOLTA, OP_SUB_VOLTA, OP_MUL_VOLTA (a conta em 2^N, igual em dev e release, em C e x64).
 *   O "estourou" sai da conta com volta r e das comparacoes (nada de flag de CPU):
 *     soma: sem sinal r < a; com sinal (b < 0) != (r < a).   sub: sem sinal a < b; com sinal (b < 0) != (a < r).
 *     mul: |a| * |b| em u64 (pelo modulo e por mul_hi) contra o limite do tipo: u8..u64 = o maximo; com sinal = 2^(N-1) - 1, ou 2^(N-1) se o produto e negativo.
 *   "alto" diz para que lado o estouro foi (saturar em MAX ou MIN): soma = b >= 0, sub = b < 0, mul = produto positivo; sem sinal: soma e mul para cima, sub para baixo.
 *   saturating = r, trocado pelo MIN/MAX se estourou. checked = ?T zerado (none) que recebe EMBRULHA(r) se nao estourou. overflowing = IR_TUPLA (r, estourou).
 *   mul_hi_u64 = metade alta do produto de 128 bits por 4 produtos de 32 bits (sem __int128: o x64 e o C usam a mesma conta; sem soma que estoure).
 * Interface (lower_ops.h): LwAritmetica (faixa EMB_WRAPPING_ADD.. em lower_call.c) e LoMulHi.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9a/L9b: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_ops.h"

enum { OP_ADD_I = 0, OP_SUB_I = 1, OP_MUL_I = 2, FAMILIA_SATURATING = 1, FAMILIA_CHECKED = 2, FAMILIA_OVERFLOWING = 3 };
enum { METADE_BITS = 32, LARGURA_MAXIMA = 64 };
#define METADE_MASCARA UINT64_C(0xFFFFFFFF)

/* O resultado com volta, se a conta estourou e para que lado (alto: bool; V_BOOL constante quando o lado e fixo). */
typedef struct { Valor resultado, estourou, alto; } Conta;

/* ---------- mul_hi_u64 ---------- */

Valor LoMulHi(const LoCtx *k, Valor a, Valor b)
{
    Valor mascara = LwValorInt((int64_t)METADE_MASCARA, TIPO_UINT), meia = LwValorInt(METADE_BITS, TIPO_UINT);
    Valor al = LoBinario(k, OP_BIT_E, TIPO_UINT, a, mascara), ah = LoBinario(k, OP_DESLOCA_DIR, TIPO_UINT, a, meia);
    Valor bl = LoBinario(k, OP_BIT_E, TIPO_UINT, b, mascara), bh = LoBinario(k, OP_DESLOCA_DIR, TIPO_UINT, b, meia);
    Valor p0 = LoBinario(k, OP_MUL, TIPO_UINT, al, bl), p1 = LoBinario(k, OP_MUL, TIPO_UINT, al, bh);
    Valor p2 = LoBinario(k, OP_MUL, TIPO_UINT, ah, bl), p3 = LoBinario(k, OP_MUL, TIPO_UINT, ah, bh);
    Valor meio = LoBinario(k, OP_SOMA, TIPO_UINT, LoBinario(k, OP_DESLOCA_DIR, TIPO_UINT, p0, meia),
                           LoBinario(k, OP_BIT_E, TIPO_UINT, p1, mascara));
    meio = LoBinario(k, OP_SOMA, TIPO_UINT, meio, LoBinario(k, OP_BIT_E, TIPO_UINT, p2, mascara));
    Valor alto = LoBinario(k, OP_SOMA, TIPO_UINT, p3, LoBinario(k, OP_DESLOCA_DIR, TIPO_UINT, p1, meia));
    alto = LoBinario(k, OP_SOMA, TIPO_UINT, alto, LoBinario(k, OP_DESLOCA_DIR, TIPO_UINT, p2, meia));
    return LoBinario(k, OP_SOMA, TIPO_UINT, alto, LoBinario(k, OP_DESLOCA_DIR, TIPO_UINT, meio, meia));
}

/* ---------- as contas ---------- */

static Valor Menor(const LoCtx *k, Valor a, Valor b)
{
    return LoBinario(k, OP_MENOR, TIPO_BOOL, a, b);
}

static Valor Diferente(const LoCtx *k, Valor a, Valor b)
{
    return LoBinario(k, OP_DIFERENTE, TIPO_BOOL, a, b);
}

static Conta ContaSoma(const LoCtx *k, TipoId tipo, Valor a, Valor b)
{
    Valor r = LoBinario(k, OP_SOMA_VOLTA, tipo, a, b);
    if (TipoEhSemSinal(tipo)) return (Conta){r, Menor(k, r, a), LwValorBool(true)};
    Valor b_negativo = Menor(k, b, LwValorInt(0, tipo));
    return (Conta){r, Diferente(k, b_negativo, Menor(k, r, a)), LoNao(k, b_negativo)};
}

static Conta ContaSub(const LoCtx *k, TipoId tipo, Valor a, Valor b)
{
    Valor r = LoBinario(k, OP_SUB_VOLTA, tipo, a, b);
    if (TipoEhSemSinal(tipo)) return (Conta){r, Menor(k, a, b), LwValorBool(false)};
    Valor b_negativo = Menor(k, b, LwValorInt(0, tipo));
    return (Conta){r, Diferente(k, b_negativo, Menor(k, a, r)), b_negativo};
}

/* |x| como u64: x < 0 vira 0 - x com volta (o minimo fica no minimo, que como sem sinal e o modulo certo), depois zero-extend. */
static Valor Magnitude(const LoCtx *k, TipoId tipo, Valor x)
{
    if (TipoEhSemSinal(tipo)) return LoConverte(k, TIPO_UINT, x);
    int32_t modulo = LoLocal(k, tipo, x);
    Valor zero = LwValorInt(0, tipo);
    IrBloco *fora = LoEntraSe(k, Menor(k, x, zero));
    LoAtribui(k, modulo, tipo, LoBinario(k, OP_SUB_VOLTA, tipo, zero, x));
    LoSaiDoSe(k, fora);
    return LoConverte(k, TIPO_UINT, LoTransmuta(k, LoSemSinal(tipo), LwValorLocal(modulo, tipo)));
}

/* O maior |produto| que cabe: sem sinal, todos os bits da largura; com sinal 2^(N-1) - 1, ou 2^(N-1) se o produto e negativo ('negativo' bool). */
static Valor LimiteDoProduto(const LoCtx *k, TipoId tipo, Valor negativo)
{
    uint32_t largura = LoLargura(k, tipo);
    uint64_t todos = largura == LARGURA_MAXIMA ? UINT64_MAX : (UINT64_C(1) << largura) - 1;
    if (TipoEhSemSinal(tipo)) return LwValorInt((int64_t)todos, TIPO_UINT);
    uint64_t positivo = UINT64_C(1) << (largura - 1);
    int32_t limite = LoLocal(k, TIPO_UINT, LwValorInt((int64_t)(positivo - 1), TIPO_UINT));
    IrBloco *fora = LoEntraSe(k, negativo);
    LoAtribui(k, limite, TIPO_UINT, LwValorInt((int64_t)positivo, TIPO_UINT));
    LoSaiDoSe(k, fora);
    return LwValorLocal(limite, TIPO_UINT);
}

static Conta ContaMul(const LoCtx *k, TipoId tipo, Valor a, Valor b)
{
    Valor r = LoBinario(k, OP_MUL_VOLTA, tipo, a, b);
    Valor ma = Magnitude(k, tipo, a), mb = Magnitude(k, tipo, b);
    Valor baixo = LoBinario(k, OP_MUL_VOLTA, TIPO_UINT, ma, mb);
    bool sinal = !TipoEhSemSinal(tipo);
    Valor zero = LwValorInt(0, tipo);
    Valor negativo = sinal ? Diferente(k, Menor(k, a, zero), Menor(k, b, zero)) : LwValorBool(false);
    Valor estourou = Menor(k, LimiteDoProduto(k, tipo, negativo), baixo);
    if (LoLargura(k, tipo) == LARGURA_MAXIMA) { /* abaixo de 64 bits o produto cabe em u64: a metade alta e zero */
        Valor alta_nao_zero = Diferente(k, LoMulHi(k, ma, mb), LwValorInt(0, TIPO_UINT));
        estourou = LoBinario(k, OP_OU, TIPO_BOOL, alta_nao_zero, estourou);
    }
    return (Conta){r, estourou, sinal ? LoNao(k, negativo) : LwValorBool(true)};
}

static Conta Calcular(const LoCtx *k, int operacao, TipoId tipo, const Valor *op)
{
    if (operacao == OP_ADD_I) return ContaSoma(k, tipo, op[0], op[1]);
    if (operacao == OP_SUB_I) return ContaSub(k, tipo, op[0], op[1]);
    return ContaMul(k, tipo, op[0], op[1]);
}

/* ---------- as familias ---------- */

/* r, trocado por MIN se estourou e por MAX se estourou para cima. */
static Valor Saturar(const LoCtx *k, TipoId tipo, const Conta *conta)
{
    int32_t saida = LoLocal(k, tipo, conta->resultado);
    IrBloco *fora = LoEntraSe(k, conta->estourou);
    LoAtribui(k, saida, tipo, LwValorInt(TipoInteiroMin(tipo), tipo));
    LoSaiDoSe(k, fora);
    fora = LoEntraSe(k, LoBinario(k, OP_E, TIPO_BOOL, conta->estourou, conta->alto));
    LoAtribui(k, saida, tipo, LwValorInt((int64_t)TipoInteiroMax(tipo), tipo));
    LoSaiDoSe(k, fora);
    return LwValorLocal(saida, tipo);
}

/* ?T: nasce none (local zerado) e recebe o resultado so se nao estourou. */
static Valor Checar(const LoCtx *k, TipoId opcional, const Conta *conta)
{
    int32_t saida = LoLocal(k, opcional, LwValorNenhum());
    IrBloco *fora = LoEntraSe(k, LoNao(k, conta->estourou));
    IrInstr *embrulho = LwNovaInstr(k->c, IR_EMBRULHA, k->pos);
    embrulho->destino = LwNovoTemp(k->c);
    embrulho->tipo = opcional;
    embrulho->a = conta->resultado;
    LoAtribui(k, saida, opcional, LwValorTemp(embrulho->destino, opcional));
    LoSaiDoSe(k, fora);
    return LwValorLocal(saida, opcional);
}

static Valor ParDoEstouro(const LoCtx *k, TipoId tupla, const Conta *conta)
{
    IrInstr *i = LwNovaInstr(k->c, IR_TUPLA, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = tupla;
    i->args = LwArgs(k->c, 2);
    i->args[0] = conta->resultado;
    i->args[1] = conta->estourou;
    i->nargs = 2;
    return LwValorTemp(i->destino, tupla);
}

static Valor Operar(const LoCtx *k, const Expr *e, const Valor *op)
{
    int relativo = e->ref_id - EMB_WRAPPING_ADD;
    int familia = relativo / EMB_ARIT_FAMILIA_TAM, operacao = relativo % EMB_ARIT_FAMILIA_TAM;
    TipoId tipo = e->args[0].valor->tipo;
    if (familia == 0) return LoBinario(k, (uint8_t)(OP_SOMA_VOLTA + operacao), tipo, op[0], op[1]);
    Conta conta = Calcular(k, operacao, tipo, op);
    if (familia == FAMILIA_SATURATING) return Saturar(k, tipo, &conta);
    if (familia == FAMILIA_CHECKED) return Checar(k, e->tipo, &conta);
    return ParDoEstouro(k, e->tipo, &conta);
}

Valor LwAritmetica(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor op[2] = {{0}};
    if (e->ref_id >= EMB_ROTATE_LEFT && e->ref_id != EMB_MUL_HI_U64) return LwBits(c, e);
    for (uint32_t i = 0; i < e->nargs; i++) op[e->args[i].indice] = LwExpr(c, e->args[i].valor);
    op[0] = LoEmLocal(&k, op[0]); /* duas constantes soltas nao viram conta de registrador */
    if (e->ref_id == EMB_MUL_HI_U64) return LoMulHi(&k, op[0], op[1]);
    return Operar(&k, e, op);
}
