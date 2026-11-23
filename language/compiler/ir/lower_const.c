/* lower_const.c - dobra de constantes no lowering: literais, constantes escalares, membros de enum e aritmetica sobre eles.
 * Versao: 0.6.3
 * Changelog: 0.6.3 - Portao Z b1: divisao real por zero constante nao dobra (IEEE pura; E0420 com mensagem propria); divisao inteira segue 0.
 *            0.6.2 - Linguagem v1.12 M3 P21v-a: DOBRAS[EX_CHAMADA_IND + 1] (sem dobra).
 *            0.6.1 -Linguagem v1.11 L12b: DobrarLiteralInteiro le o indice de v.x (EX_INT com REF_COMPONENTE) de ref_id.
 *            0.6.0 - Linguagem v1.11 L9: DobrarChamada (size_of, align_of e offset_of dobram em int; nas const e em static_assert).
 *            0.5.0 - Linguagem v1.11 L1: DobrarLiteralInteiro devolve Valor real quando o tipo anotado e real (literal inteiro em contexto real).
 *            0.4.0 - Linguagem v1.7 P10: versao de fechamento da v1.7 (sem mudanca de logica).
 *            0.3.1 - Linguagem v1.7 P1: DOBRAS[EX_FTEXTO + 1] (as 4 expressoes novas sem dobra = NULL).
 *            0.3.0 - Linguagem v1.6 P1a: DOBRAS[EX_TIPO + 1] (EX_TIPO vazio = nao dobra).
 *            0.2.3 - Linguagem v1.5 P1a: DOBRAS[EX_LITERAL_ARRAY + 1] (entradas novas vazias = nao dobra).
 *            0.2.2 - Linguagem v1.3 P6: DOBRAS[EX_TEXTO] (const str e cstr).
 *            0.2.1 - Linguagem v1.3: DOBRAS[EX_NONE + 1] (forma da AST; EX_NONE nao dobra).
 *            0.2.0 - Linguagem v1.2: dobra por largura e sinal (i8..u64, int, uint), f64 em double, bits (& | ~ << >> e '~' unario) so em
 *                    sem sinal, 'as' entre numericos (EX_CONVERSAO), '-9223372036854775808' (literal negado lido como magnitude).
 *                    Valor.inteiro guarda BITS: tipo sem sinal le (uint64_t)inteiro. E0420 cita o tipo e a faixa.
 *            0.1.0 - Fase 5d: primeira versao. f32 em float (igual ao runtime), divisao e resto por zero = 0, truncamento de
 *                    inteiro; resultado que estoura o tipo = E0420 (nunca saida silenciosa).
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include <math.h>
#include <stdlib.h>

#include "ir/lower_interno.h"

typedef bool (*DobraNo)(LowerCtx *c, const Expr *e, Valor *saida);
typedef bool (*OperacaoInteira)(int64_t x, int64_t y, int64_t *resultado);
typedef bool (*OperacaoSemSinal)(uint64_t x, uint64_t y, uint64_t *resultado);
typedef float (*OperacaoReal)(float x, float y);
typedef double (*OperacaoReal64)(double x, double y);

static bool DobrarNo(LowerCtx *c, const Expr *e, Valor *saida);

/* Tipo que a semantica deu ao no; sem ele o lowering nao sabe o que dobrar. */
static TipoId TipoAnotado(const Expr *e)
{
    if (e->tipo == TIPO_NAO_RESOLVIDO) FalhaInterna("lowering: expressao sem tipo (offset %u)", (unsigned)e->pos_op);
    return e->tipo;
}

/* E0420: o 1o diagnostico fica; devolve false para uso em "return Estouro(...)". */
static bool Estouro(LowerCtx *c, const Expr *e)
{
    TipoId tipo = e->tipo;

    if (c->sem->diag->tem) return false;
    if (TipoEhInteiro(tipo)) {
        DiagRelatar(c->sem->diag, E0420, e->pos_op, "o valor constante nao cabe no tipo %s (%lld..=%llu)", TipoNomeFixo(tipo),
                    (long long)TipoInteiroMin(tipo), (unsigned long long)TipoInteiroMax(tipo));
    } else {
        DiagRelatar(c->sem->diag, E0420, e->pos_op, "o valor constante nao cabe no tipo %s", tipo == TIPO_F64 ? "f64" : "f32");
    }
    return false;
}

static uint64_t MascaraDaLargura(uint8_t largura)
{
    return largura >= 64 ? UINT64_MAX : ((UINT64_C(1) << largura) - 1);
}

/* ---------- folhas ---------- */

/* So digitos decimais; o sinal e um OP_NEGAR por cima. Os bits do Valor sao a magnitude como uint64_t. */
static bool DobrarLiteralInteiro(LowerCtx *c, const Expr *e, Valor *saida)
{
    uint64_t mag;
    TipoId tipo = TipoAnotado(e);

    if (e->ref == REF_COMPONENTE) { /* v1.11 L12b: indice criado por v.x; o valor mora em ref_id (0..3), nao no fonte */
        *saida = LwValorInt(e->ref_id, tipo);
        return true;
    }
    if (TipoEhReal(&c->sem->tipos, tipo)) { /* v1.11: literal inteiro em contexto real sai como o real de mesmo valor (exato: a semantica conferiu) */
        if (!TipoLerLiteralU64(c->sem->fonte->texto + e->pos_op, &mag)) return Estouro(c, e);
        *saida = LwValorReal(tipo == TIPO_F64 ? (double)mag : (double)(float)mag, tipo);
        return true;
    }
    if (!TipoLerLiteralU64(c->sem->fonte->texto + e->pos_op, &mag) || !TipoInteiroCabe(tipo, false, mag)) return Estouro(c, e);
    *saida = LwValorInt((int64_t)mag, tipo);
    return true;
}

/* Real f32 arredonda para float, como o C gerado fara com o literal; f64 fica em double. */
static bool DobrarLiteralReal(LowerCtx *c, const Expr *e, Valor *saida)
{
    TipoId tipo = TipoAnotado(e);
    double v = strtod(c->sem->fonte->texto + e->pos_op, NULL);

    if (tipo != TIPO_F64) v = (double)(float)v;
    if (!isfinite(v)) return Estouro(c, e);
    *saida = LwValorReal(v, tipo);
    return true;
}

static bool DobrarLiteralBool(LowerCtx *c, const Expr *e, Valor *saida)
{
    (void)c;
    *saida = LwValorBool(e->op != 0);
    return true;
}

/* Const escalar ja dobrada; const de struct nao e constante de valor (o uso refaz o lowering do init). */
static bool DobrarLiteralTexto(LowerCtx *c, const Expr *e, Valor *saida)
{
    *saida = LwValorTexto(LwTextoDecodificado(c, e), e->tipo);
    return true;
}

static bool DobrarNome(LowerCtx *c, const Expr *e, Valor *saida)
{
    if (e->ref != REF_CONST || !c->const_escalar[e->ref_id]) return false;
    *saida = c->const_valor[e->ref_id];
    return true;
}

static bool DobrarCampo(LowerCtx *c, const Expr *e, Valor *saida)
{
    (void)c;
    if (e->ref != REF_MEMBRO) return false;
    *saida = LwValorInt(e->ref_id, TipoAnotado(e));
    return true;
}

/* size_of, align_of e offset_of valem como constante (v1.11 L9); qualquer outra chamada nao e constante. */
static bool DobrarChamada(LowerCtx *c, const Expr *e, Valor *saida)
{
    int64_t valor;
    if (e->ref != REF_EMBUTIDO || e->ref_id < EMB_SIZE_OF || e->ref_id > EMB_OFFSET_OF) return false;
    if (!LwLayoutValor(c, e, &valor)) return false;
    *saida = LwValorInt(valor, TIPO_INT);
    return true;
}

/* ---------- unario ---------- */

static bool DobrarNegacao(LowerCtx *c, const Expr *e, Valor a, Valor *saida)
{
    if (a.tipo_no == V_REAL) {
        *saida = LwValorReal(a.tipo == TIPO_F64 ? -a.real : (double)(-(float)a.real), a.tipo);
        return true;
    }
    if (TipoEhSemSinal(a.tipo)) { /* a semantica ja recusou '-x'; so 0 sobra, de uma dobra como '0 - 0' */
        if (a.inteiro != 0) return Estouro(c, e);
        *saida = LwValorInt(0, a.tipo);
        return true;
    }
    if (a.inteiro == INT64_MIN || !TipoBitsCabem(a.tipo, -a.inteiro)) return Estouro(c, e);
    *saida = LwValorInt(-a.inteiro, a.tipo);
    return true;
}

/* '-literal': le a magnitude e confere contra o tipo ja com o sinal, entao -128 cabe em i8 e -9223372036854775808 em int. */
static bool DobrarLiteralNegado(LowerCtx *c, const Expr *e, Valor *saida)
{
    uint64_t mag;
    TipoId tipo = TipoAnotado(e);

    if (!TipoLerLiteralU64(c->sem->fonte->texto + e->a->pos_op, &mag) || !TipoInteiroCabe(tipo, true, mag)) return Estouro(c, e);
    *saida = LwValorInt((int64_t)(UINT64_C(0) - mag), tipo);
    return true;
}

static bool DobrarUnario(LowerCtx *c, const Expr *e, Valor *saida)
{
    Valor a;
    if (e->op == OP_NEGAR && e->a->tipo_no == EX_INT && TipoEhInteiro(e->tipo)) return DobrarLiteralNegado(c, e, saida);
    if ((e->op != OP_NEGAR && e->op != OP_NAO && e->op != OP_BIT_NAO) || !DobrarNo(c, e->a, &a)) return false;
    if (e->op == OP_NAO) {
        *saida = LwValorBool(a.inteiro == 0);
        return true;
    }
    if (e->op == OP_BIT_NAO) { /* so sem sinal: o complemento fica dentro da largura do tipo */
        *saida = LwValorInt((int64_t)(~(uint64_t)a.inteiro & MascaraDaLargura(TipoLarguraBits(&c->sem->tipos, a.tipo))), a.tipo);
        return true;
    }
    return DobrarNegacao(c, e, a, saida);
}

/* ---------- binario ---------- */

static bool InteiroSoma(int64_t x, int64_t y, int64_t *r) { return !__builtin_add_overflow(x, y, r); }
static bool InteiroSub(int64_t x, int64_t y, int64_t *r) { return !__builtin_sub_overflow(x, y, r); }
static bool InteiroMul(int64_t x, int64_t y, int64_t *r) { return !__builtin_mul_overflow(x, y, r); }

static bool InteiroDiv(int64_t x, int64_t y, int64_t *r)
{
    if (y == 0) *r = 0;
    else if (x == INT64_MIN && y == -1) return false;
    else *r = x / y;
    return true;
}

static bool InteiroResto(int64_t x, int64_t y, int64_t *r)
{
    *r = (y == 0 || y == -1) ? 0 : x % y;
    return true;
}

static bool SemSinalSoma(uint64_t x, uint64_t y, uint64_t *r) { return !__builtin_add_overflow(x, y, r); }
static bool SemSinalSub(uint64_t x, uint64_t y, uint64_t *r) { return !__builtin_sub_overflow(x, y, r); } /* 0 - 1 estoura */
static bool SemSinalMul(uint64_t x, uint64_t y, uint64_t *r) { return !__builtin_mul_overflow(x, y, r); }

static bool SemSinalDiv(uint64_t x, uint64_t y, uint64_t *r)
{
    *r = y == 0 ? 0 : x / y;
    return true;
}

static bool SemSinalResto(uint64_t x, uint64_t y, uint64_t *r)
{
    *r = y == 0 ? 0 : x % y;
    return true;
}

static float RealSoma(float x, float y) { return x + y; }
static float RealSub(float x, float y) { return x - y; }
static float RealMul(float x, float y) { return x * y; }
static float RealDiv(float x, float y) { return x / y; } /* IEEE pura: divisor zero da inf/nan, que DobrarAritmeticaReal recusa com E0420 */

static double Real64Soma(double x, double y) { return x + y; }
static double Real64Sub(double x, double y) { return x - y; }
static double Real64Mul(double x, double y) { return x * y; }
static double Real64Div(double x, double y) { return x / y; } /* IEEE pura: divisor zero da inf/nan, que DobrarAritmeticaReal recusa com E0420 */

/* Indexadas por op - OP_SOMA (OP_SOMA, OP_SUB, OP_MUL, OP_DIV, OP_RESTO); o resto nao existe em real. */
static const OperacaoInteira OPERACOES_INTEIRAS[] = {InteiroSoma, InteiroSub, InteiroMul, InteiroDiv, InteiroResto};
static const OperacaoSemSinal OPERACOES_SEM_SINAL[] = {SemSinalSoma, SemSinalSub, SemSinalMul, SemSinalDiv, SemSinalResto};
static const OperacaoReal OPERACOES_REAIS[] = {RealSoma, RealSub, RealMul, RealDiv};
static const OperacaoReal64 OPERACOES_REAIS64[] = {Real64Soma, Real64Sub, Real64Mul, Real64Div};

static bool DobrarAritmeticaInteira(LowerCtx *c, const Expr *e, Valor a, Valor b, Valor *saida)
{
    int64_t r;
    uint64_t ru;
    TipoId tipo = TipoAnotado(e);

    if (TipoEhSemSinal(tipo)) {
        if (!OPERACOES_SEM_SINAL[e->op - OP_SOMA]((uint64_t)a.inteiro, (uint64_t)b.inteiro, &ru)) return Estouro(c, e);
        r = (int64_t)ru;
    } else if (!OPERACOES_INTEIRAS[e->op - OP_SOMA](a.inteiro, b.inteiro, &r)) {
        return Estouro(c, e);
    }
    if (!TipoBitsCabem(tipo, r)) return Estouro(c, e);
    *saida = LwValorInt(r, tipo);
    return true;
}

static bool DobrarAritmeticaReal(LowerCtx *c, const Expr *e, Valor a, Valor b, Valor *saida)
{
    TipoId tipo = TipoAnotado(e);
    double r;

    if (e->op > OP_DIV) return false;
    if (e->op == OP_DIV && b.real == 0.0) {
        /* Divisao real por zero constante nao dobra: a mensagem diz o que escrever no lugar. */
        if (!c->sem->diag->tem)
            DiagRelatar(c->sem->diag, E0420, e->pos_op, "divisao real por zero numa constante; use math.inf()/math.nan() ou um divisor variavel");
        return false;
    }
    if (tipo == TIPO_F64) r = OPERACOES_REAIS64[e->op - OP_SOMA](a.real, b.real);
    else r = (double)OPERACOES_REAIS[e->op - OP_SOMA]((float)a.real, (float)b.real);
    if (!isfinite(r)) return Estouro(c, e);
    *saida = LwValorReal(r, tipo);
    return true;
}

/* Bits so em sem sinal. Deslocar pela largura do tipo ou mais da 0 (sem trap); os bits que saem pela esquerda se perdem. */
static bool DobrarBits(LowerCtx *c, const Expr *e, Valor a, Valor b, Valor *saida)
{
    TipoId tipo = TipoAnotado(e);
    uint8_t largura = TipoLarguraBits(&c->sem->tipos, tipo);
    uint64_t x = (uint64_t)a.inteiro, y = (uint64_t)b.inteiro, r;

    switch (e->op) {
    case OP_BIT_E: r = x & y; break;
    case OP_BIT_OU: r = x | y; break;
    case OP_BIT_XOU: r = x ^ y; break;
    case OP_DESLOCA_ESQ: r = y >= largura ? 0 : (x << y) & MascaraDaLargura(largura); break;
    default: r = y >= largura ? 0 : x >> y; break;
    }
    *saida = LwValorInt((int64_t)r, tipo);
    return true;
}

/* Ordem de a em relacao a b: -1, 0 ou 1 (numero, bool e enum; o enum compara o indice do membro; sem sinal compara como uint64). */
static int OrdemDe(Valor a, Valor b)
{
    if (a.tipo_no == V_REAL) return (a.real > b.real) - (a.real < b.real);
    if (TipoEhSemSinal(a.tipo)) return ((uint64_t)a.inteiro > (uint64_t)b.inteiro) - ((uint64_t)a.inteiro < (uint64_t)b.inteiro);
    return (a.inteiro > b.inteiro) - (a.inteiro < b.inteiro);
}

/* Indexada por op - OP_IGUAL; o bit (ordem + 1) diz se a comparacao vale naquela ordem. */
static const uint8_t ORDENS_ACEITAS[] = {
    [OP_IGUAL - OP_IGUAL] = 2,        /* == : so ordem 0 */
    [OP_DIFERENTE - OP_IGUAL] = 5,    /* != : ordens -1 e 1 */
    [OP_MENOR - OP_IGUAL] = 1,        /* <  : ordem -1 */
    [OP_MENOR_IGUAL - OP_IGUAL] = 3,  /* <= : ordens -1 e 0 */
    [OP_MAIOR - OP_IGUAL] = 4,        /* >  : ordem 1 */
    [OP_MAIOR_IGUAL - OP_IGUAL] = 6,  /* >= : ordens 0 e 1 */
};

static Valor DobrarComparacao(uint8_t op, Valor a, Valor b)
{
    return LwValorBool(((ORDENS_ACEITAS[op - OP_IGUAL] >> (OrdemDe(a, b) + 1)) & 1) != 0);
}

static Valor DobrarLogico(uint8_t op, Valor a, Valor b)
{
    return LwValorBool(op == OP_E ? (a.inteiro != 0 && b.inteiro != 0) : (a.inteiro != 0 || b.inteiro != 0));
}

static bool DobrarBinario(LowerCtx *c, const Expr *e, Valor *saida)
{
    Valor a, b;
    if (!DobrarNo(c, e->a, &a) || !DobrarNo(c, e->b, &b)) return false;
    if (e->op == OP_E || e->op == OP_OU) *saida = DobrarLogico(e->op, a, b);
    else if (e->op <= OP_MAIOR_IGUAL) *saida = DobrarComparacao(e->op, a, b);
    else if (e->op >= OP_BIT_E) return DobrarBits(c, e, a, b, saida); /* antes de indexar as tabelas por op - OP_SOMA */
    else if (a.tipo_no == V_REAL) return DobrarAritmeticaReal(c, e, a, b, saida);
    else return DobrarAritmeticaInteira(c, e, a, b, saida);
    return true;
}

/* ---------- conversao 'as' (so chega aqui em const e em arvore constante) ---------- */

/* real -> inteiro: trunca para zero; NaN ou fora da faixa do destino = false (o chamador relata E0420). */
static bool RealParaInteiro(double real, TipoId alvo, int64_t *bits)
{
    double t = trunc(real);

    if (isnan(t) || t < -9223372036854775808.0 || t >= 18446744073709551616.0) return false;
    if (t < 0) {
        if (TipoEhSemSinal(alvo) || !TipoBitsCabem(alvo, (int64_t)t)) return false;
        *bits = (int64_t)t;
        return true;
    }
    uint64_t magnitude = t >= 9223372036854775808.0 ? (uint64_t)(t - 9223372036854775808.0) + UINT64_C(0x8000000000000000) : (uint64_t)t;
    if (!TipoInteiroCabe(alvo, false, magnitude)) return false;
    *bits = (int64_t)magnitude;
    return true;
}

/* inteiro -> inteiro: o valor matematico da origem (com ou sem sinal) precisa caber no destino. */
static bool InteiroParaInteiro(Valor a, TipoId alvo, int64_t *bits)
{
    bool negativo = !TipoEhSemSinal(a.tipo) && a.inteiro < 0;
    uint64_t magnitude = negativo ? UINT64_C(0) - (uint64_t)a.inteiro : (uint64_t)a.inteiro;

    if (!TipoInteiroCabe(alvo, negativo, magnitude)) return false;
    *bits = (int64_t)(negativo ? UINT64_C(0) - magnitude : magnitude);
    return true;
}

static bool DobrarConversao(LowerCtx *c, const Expr *e, Valor *saida)
{
    Valor a;
    TipoId alvo = TipoAnotado(e);
    int64_t bits;
    double real;

    if (!DobrarNo(c, e->a, &a)) return false;
    if (TipoEhInteiro(alvo)) {
        bool cabe = a.tipo_no == V_REAL ? RealParaInteiro(a.real, alvo, &bits) : InteiroParaInteiro(a, alvo, &bits);
        if (!cabe) return Estouro(c, e);
        *saida = LwValorInt(bits, alvo);
        return true;
    }
    real = a.tipo_no == V_REAL ? a.real : (TipoEhSemSinal(a.tipo) ? (double)(uint64_t)a.inteiro : (double)a.inteiro);
    if (alvo != TIPO_F64) real = (double)(float)real;
    if (!isfinite(real)) return Estouro(c, e);
    *saida = LwValorReal(real, alvo);
    return true;
}

/* ---------- despacho ---------- */

static const DobraNo DOBRAS[EX_CHAMADA_IND + 1] = {
    [EX_INT] = DobrarLiteralInteiro, [EX_REAL] = DobrarLiteralReal, [EX_BOOL] = DobrarLiteralBool, [EX_TEXTO] = DobrarLiteralTexto,
    [EX_NOME] = DobrarNome, [EX_CAMPO] = DobrarCampo, [EX_UNARIO] = DobrarUnario, [EX_BINARIO] = DobrarBinario,
    [EX_CONVERSAO] = DobrarConversao, [EX_CHAMADA] = DobrarChamada,
};

/* false sem diagnostico = o no nao e constante. */
static bool DobrarNo(LowerCtx *c, const Expr *e, Valor *saida)
{
    return DOBRAS[e->tipo_no] != NULL && DOBRAS[e->tipo_no](c, e, saida);
}

bool LwDobrar(LowerCtx *c, const Expr *e, Valor *saida)
{
    if (!DobrarNo(c, e, saida)) return false;
    if (saida->tipo_no == V_INT && !TipoBitsCabem(saida->tipo, saida->inteiro)) return Estouro(c, e);
    return true;
}
