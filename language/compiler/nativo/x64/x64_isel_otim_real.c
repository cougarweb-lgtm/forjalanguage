/* x64_isel_otim_real.c - selecao pos-alocador (M7a F5) de real: ADD SUB MUL DIV, NEG, RAIZ, CONST, COPIA e as conversoes calculados no xmm do proprio dst, com os operandos
 * nos xmm que ja os guardam (sem passar por xmm0/xmm1), e o ucomis das comparacoes fundidas com o desvio.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - R5 (SIMD): IselOtimSelecionaReal (SELECIONA real = minss/maxss/minsd/maxsd).
 *            0.1.0 - M7a F5: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F4> --release-muda
 *
 * Mesma regra do inteiro: dst e operando de vregs diferentes nunca dividem xmm, entao `movaps d,a ; op d,b` nao destroi b. Se b mora no xmm do dst (dst e b o mesmo vreg) a conta
 * comutativa troca os operandos e a outra vai por xmm0 e guarda. xmm0/xmm1 sao rascunho; o literal real continua passando por r11 (IselCarregarReal).
 */
#include "nativo/x64/x64_isel_otim.h"

#define XMM_A 0u /* rascunho do resultado */
#define XMM_B 1u /* rascunho do segundo operando */

/* xmm com o valor do operando real: o proprio, se ja mora em um; senao carregado no rascunho. */
static uint8_t XmmFonte(X64Ctx *c, const BirOperando *o, uint8_t rascunho, bool *ok)
{
    int x = IselOtimRegXmm(c, o);

    if (x >= 0) {
        *ok = true;
        return (uint8_t)x;
    }
    *ok = IselCarregarReal(c, rascunho, o);
    return rascunho;
}

/* xmm = o; nada se o valor ja mora la. */
static bool XmmCarregarEm(X64Ctx *c, uint8_t xmm, const BirOperando *o)
{
    if (IselOtimRegXmm(c, o) == (int)xmm)
        return true;
    return IselCarregarReal(c, xmm, o);
}

static uint8_t XmmDoResultado(const X64Ctx *c, uint32_t dst)
{
    int x = IselOtimRegXmmDestino(c, dst);

    return x >= 0 ? (uint8_t)x : (uint8_t)XMM_A;
}

/* Resultado calculado em xmm0 (dst fora de registrador ou em conflito): guarda no dst. */
static int32_t FecharReal(X64Ctx *c, uint32_t dst, uint8_t xmm, bool ok)
{
    if (!ok)
        return -1;
    if (xmm != XMM_A)
        return 1;
    return IselGuardarReal(c, dst, (uint8_t)XMM_A) ? 1 : -1;
}

/* ---- contas ---- */

static uint16_t FormaDaConta(uint8_t op, bool dupla)
{
    switch (op) {
    case BIR_ADD: return dupla ? FX_ADDSD_XX : FX_ADDSS_XX;
    case BIR_SUB: return dupla ? FX_SUBSD_XX : FX_SUBSS_XX;
    case BIR_MUL: return dupla ? FX_MULSD_XX : FX_MULSS_XX;
    default: return dupla ? FX_DIVSD_XX : FX_DIVSS_XX;
    }
}

static int32_t ContaReal(X64Ctx *c, const BirInstr *i)
{
    const BirOperando *a = &i->a, *b = &i->b;
    uint8_t d = XmmDoResultado(c, i->dst), xb;
    bool comutativa = i->op == BIR_ADD || i->op == BIR_MUL;
    bool ok;

    if (d != XMM_A && IselOtimRegXmm(c, b) == (int)d && !(a->tipo == BO_VREG && b->tipo == BO_VREG && a->id == b->id)) {
        if (comutativa) {
            a = &i->b;
            b = &i->a;
        } else
            d = (uint8_t)XMM_A; /* b mora no xmm do dst: calcula em xmm0 e guarda */
    }
    if (!XmmCarregarEm(c, d, a))
        return -1;
    xb = XmmFonte(c, b, (uint8_t)XMM_B, &ok);
    if (!ok)
        return -1;
    return FecharReal(c, i->dst, d, X64Emitir(c, MI2(FormaDaConta(i->op, i->classe == BC_F64), MXmm(d), MXmm(xb))));
}

/* -a = a xor mascara de sinal em .rdata (o endereco passa por rax: [rip+d] so existe no LEA). */
static int32_t NegReal(X64Ctx *c, const BirInstr *i)
{
    uint8_t d = XmmDoResultado(c, i->dst);
    MOperando mascara = MMem(X64_RIP, 0, 64);

    mascara.simbolo = c->obj_sim[i->b.id];
    if (!XmmCarregarEm(c, d, &i->a) || !X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RAX, 64), mascara)))
        return -1;
    return FecharReal(c, i->dst, d, X64Emitir(c, MI2(FX_XORPS_XM, MXmm(d), MMem(X64_RAX, 0, 128))));
}

static int32_t RaizReal(X64Ctx *c, const BirInstr *i)
{
    uint8_t d = XmmDoResultado(c, i->dst), x;
    bool ok;

    x = XmmFonte(c, &i->a, d, &ok);
    if (!ok)
        return -1;
    return FecharReal(c, i->dst, d, X64Emitir(c, MI2(i->classe == BC_F64 ? FX_SQRTSD_XX : FX_SQRTSS_XX, MXmm(d), MXmm(x))));
}

/* CONST/COPIA: 0.0 vira xorps d,d; o resto reusa a carga (movaps de registrador, movss/movsd do slot, literal por r11). */
static int32_t MoverReal(X64Ctx *c, const BirInstr *i)
{
    int x = IselOtimRegXmmDestino(c, i->dst);
    bool zero = i->a.tipo == BO_IMED && (i->classe == BC_F64 ? i->a.imed == 0 : (uint32_t)i->a.imed == 0u);

    if (x < 0 || (i->op != BIR_CONST && i->op != BIR_COPIA) || i->a.classe != i->classe)
        return 0;
    if (zero)
        return X64Emitir(c, MI2(FX_XORPS_XX, MXmm((uint8_t)x), MXmm((uint8_t)x))) ? 1 : -1;
    return XmmCarregarEm(c, (uint8_t)x, &i->a) ? 1 : -1;
}

/* SELECIONA real no padrao minss/maxss (BirSelecionaMinMax, que o validador exige): d = primeiro ; d = min/max d, segundo (o 2o operando e o que sai com NaN ou +-0, como no BIR).
 * Mesma regra da ContaReal: se o segundo mora no xmm do dst (outro vreg que nao o primeiro), calcula em xmm0 e guarda. */
bool IselOtimSelecionaReal(X64Ctx *c, const BirInstr *i)
{
    const BirOperando *primeiro, *segundo;
    bool maximo, dupla = i->classe == BC_F64, ok;
    uint8_t d, xs;
    uint16_t forma;

    if (!BirSelecionaMinMax(i, &maximo, &primeiro, &segundo))
        return X64SemRegra(c, "seleciona", "real fora do padrao minss/maxss");
    d = XmmDoResultado(c, i->dst);
    if (d != XMM_A && IselOtimRegXmm(c, segundo) == (int)d && !(primeiro->tipo == BO_VREG && segundo->tipo == BO_VREG && primeiro->id == segundo->id))
        d = (uint8_t)XMM_A;
    if (!XmmCarregarEm(c, d, primeiro))
        return false;
    xs = XmmFonte(c, segundo, (uint8_t)XMM_B, &ok);
    if (!ok)
        return false;
    forma = maximo ? (dupla ? FX_MAXSD_XX : FX_MAXSS_XX) : (dupla ? FX_MINSD_XX : FX_MINSS_XX);
    return FecharReal(c, i->dst, d, X64Emitir(c, MI2(forma, MXmm(d), MXmm(xs)))) > 0;
}

/* ---- conversoes ---- */

/* dst = (real) a, a em I32 ou I64; o I32 sem sinal usa a forma de 64 bits (o registrador vive zero-extendido). */
static int32_t InteiroParaReal(X64Ctx *c, const BirInstr *i)
{
    bool alarga = i->a.classe == BC_I32 && i->aux == BIR_SEM_SINAL;
    bool r64 = i->a.classe == BC_I64 || alarga;
    bool dupla = i->classe == BC_F64;
    uint8_t d = XmmDoResultado(c, i->dst);
    uint16_t forma = dupla ? (r64 ? FX_CVTSI2SD_XR64 : FX_CVTSI2SD_XR32) : (r64 ? FX_CVTSI2SS_XR64 : FX_CVTSI2SS_XR32);
    int ra = IselOtimRegInt(c, &i->a);
    X64Reg fonte = ra >= 0 ? (X64Reg)ra : X64_RAX;

    if (i->a.classe != BC_I32 && i->a.classe != BC_I64)
        return 0;
    if (ra < 0 && !IselCarregar(c, X64_RAX, &i->a, alarga ? BE_ZERO : BE_NENHUMA))
        return -1;
    return FecharReal(c, i->dst, d, X64Emitir(c, MI2(forma, MXmm(d), MReg(fonte, r64 ? 64 : 32))));
}

static int32_t RealParaReal(X64Ctx *c, const BirInstr *i)
{
    uint8_t d = XmmDoResultado(c, i->dst), x;
    uint16_t forma = i->classe == BC_F64 ? FX_CVTSS2SD_XX : FX_CVTSD2SS_XX;
    bool ok;

    x = XmmFonte(c, &i->a, d, &ok);
    if (!ok)
        return -1;
    return FecharReal(c, i->dst, d, X64Emitir(c, MI2(forma, MXmm(d), MXmm(x))));
}

int32_t IselOtimReal(X64Ctx *c, const BirInstr *i)
{
    switch (i->op) {
    case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_DIV: return ContaReal(c, i);
    case BIR_NEG: return NegReal(c, i);
    case BIR_RAIZ: return RaizReal(c, i);
    case BIR_CONST: case BIR_COPIA: return MoverReal(c, i);
    case BIR_I_PARA_F: return InteiroParaReal(c, i);
    case BIR_F_PARA_F: return RealParaReal(c, i);
    default: return 0;
    }
}

/* ---- ucomis para o desvio fundido ---- */

/* So as condicoes sem paridade: FLT e FLE comparam b com a (trocados), FGT e FGE a com b; o salto e A (>) ou AE (>=). */
int32_t IselOtimCmpReal(X64Ctx *c, const BirInstr *i, X64Cond *cc)
{
    bool troca = i->aux == BCOND_FLT || i->aux == BCOND_FLE;
    const BirOperando *esq = troca ? &i->b : &i->a;
    const BirOperando *dir = troca ? &i->a : &i->b;
    uint8_t xe, xd;
    bool ok;

    if (i->aux < BCOND_FLT || i->aux > BCOND_FGE)
        return 0; /* FEQ e FNE pedem a paridade do NaN: ficam na selecao comum */
    *cc = (i->aux == BCOND_FLT || i->aux == BCOND_FGT) ? X64_CC_A : X64_CC_AE;
    xe = XmmFonte(c, esq, (uint8_t)XMM_A, &ok);
    if (!ok)
        return -1;
    xd = XmmFonte(c, dir, (uint8_t)XMM_B, &ok);
    if (!ok)
        return -1;
    return X64Emitir(c, MI2(i->a.classe == BC_F64 ? FX_UCOMISD_XX : FX_UCOMISS_XX, MXmm(xe), MXmm(xd))) ? 1 : -1;
}
