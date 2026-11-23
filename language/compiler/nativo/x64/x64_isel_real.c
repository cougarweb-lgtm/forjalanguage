/* x64_isel_real.c - selecao de instrucoes de real (f32 e f64) no degrau 0: carga e guarda de xmm, ADD SUB MUL DIV, NEG, CMP, I_PARA_F e F_PARA_F.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - S2 (D0703): vetor V128 (4 x f32, 2 x f64): IselCarregarVetor, IselGuardarVetor, IselContaVetorial (addps..divpd), IselSplat (shufps/unpcklpd), IselCopiaVetor, IselCarregaVetor e IselGravaVetor (movups).
 *            0.2.2 - M7a F2: carga e guarda de vreg real por AlocOperandoDe/AlocCarregar/AlocMemoria.
 *            0.2.1 - M3 F9b-1: decisoes extraidas de IselContaReal, IselCmpReal e IselIParaF.
 *            0.2.0 - Linguagem v1.11 L13: IselRaiz (BIR_RAIZ = sqrtss/sqrtsd xmm0, xmm0).
 *            0.1.0 - M2 P10b: todo vreg real tem um slot de 8 bytes ([rbp-8n]; f32 usa 4); carrega em xmm0 e xmm1, opera, guarda de xmm0. Literal real: mov r11,bits + movd/movq.
 *                    Comparacao por ucomis (== "ZF=1 e PF=0", != "ZF=0 ou PF=1"; < e <= trocam os operandos e usam seta/setae, que o NaN deixa falsos).
 *                    NEG = xorps com a mascara de sinal em .rdata (lea rax,[rip+mascara]; xorps xmm0,[rax]). I_PARA_F: cvtsi2ss/sd de 32 bits (i32) ou 64 bits
 *                    (int, e u32 depois do mov que zera o alto). F_PARA_F: cvtss2sd e cvtsd2ss. Rascunho: xmm0, xmm1, rax, rcx e r11 (o literal).
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/real
 *
 * O numero do registrador que IselCarregar e IselGuardar recebem vale como xmm quando o operando e real (X64_RAX == 0 == xmm0): assim o retorno, o argumento
 * k < 4 (xmm<k>) e o CONST/COPIA reutilizam o mesmo caminho dos inteiros.
 */
#include "nativo/x64/x64_interno.h"

#define XMM_A 0u /* primeiro operando e resultado */
#define XMM_B 1u /* segundo operando */

/* ---- carga e guarda ---- */

/* Literal: os bits passam por r11 (f32 em 32 bits, f64 em 64) e vao ao xmm; nada vive em .rdata. */
static bool CarregarImediatoReal(X64Ctx *c, uint8_t xmm, const BirOperando *o)
{
    if (o->classe == BC_F64)
        return X64Emitir(c, MI2(FX_MOV64_RI, MReg(X64_R11, 64), MImed(o->imed))) &&
               X64Emitir(c, MI2(FX_MOVQ_XR, MXmm(xmm), MReg(X64_R11, 64)));
    return X64Emitir(c, MI2(FX_MOV32_RI, MReg(X64_R11, 32), MImed((int32_t)o->imed))) &&
           X64Emitir(c, MI2(FX_MOVD_XR, MXmm(xmm), MReg(X64_R11, 32)));
}

bool IselCarregarReal(X64Ctx *c, uint8_t xmm, const BirOperando *o)
{
    bool dupla = o->classe == BC_F64;
    AlocOperando vreg = AlocOperandoDe(c->f, o->id, AlocBits(o->classe));

    if (o->tipo == BO_IMED)
        return CarregarImediatoReal(c, xmm, o);
    if (o->tipo != BO_VREG)
        return X64SemRegra(c, "operando", "esperava vreg ou imediato real");
    return AlocCarregar(c, dupla ? FX_MOVSD_XM : FX_MOVSS_XM, MXmm(xmm), &vreg);
}

bool IselGuardarXmm(X64Ctx *c, uint8_t classe, MOperando memoria, uint8_t xmm)
{
    return X64Emitir(c, MI2(classe == BC_F64 ? FX_MOVSD_MX : FX_MOVSS_MX, memoria, MXmm(xmm)));
}

bool IselGuardarReal(X64Ctx *c, uint32_t vreg, uint8_t xmm)
{
    uint8_t classe = c->f->classe_vreg[vreg];

    AlocOperando destino = AlocOperandoDe(c->f, vreg, AlocBits(classe));

    return AlocGravar(c, classe == BC_F64 ? FX_MOVSD_MX : FX_MOVSS_MX, &destino, MXmm(xmm));
}

/* ---- contas ---- */

typedef struct { uint8_t op; uint16_t ss, sd; } ContaReal;

static const ContaReal CONTAS_REAIS[] = {
    {BIR_ADD, FX_ADDSS_XX, FX_ADDSD_XX}, {BIR_SUB, FX_SUBSS_XX, FX_SUBSD_XX},
    {BIR_MUL, FX_MULSS_XX, FX_MULSD_XX}, {BIR_DIV, FX_DIVSS_XX, FX_DIVSD_XX},
};

static bool EmitirContaReal(X64Ctx *c, const BirInstr *i, const ContaReal *conta)
{
    if (!IselCarregarReal(c, XMM_A, &i->a) || !IselCarregarReal(c, XMM_B, &i->b))
        return false;
    if (!X64Emitir(c, MI2(i->classe == BC_F64 ? conta->sd : conta->ss, MXmm(XMM_A), MXmm(XMM_B))))
        return false;
    return IselGuardarReal(c, i->dst, XMM_A);
}

/* xmm0 = a op b, sem checagem (divisao por zero da inf ou NaN, como o C). */
bool IselContaReal(X64Ctx *c, const BirInstr *i)
{
    for (uint32_t k = 0; k < sizeof CONTAS_REAIS / sizeof CONTAS_REAIS[0]; k++) {
        if (CONTAS_REAIS[k].op == i->op)
            return EmitirContaReal(c, i, &CONTAS_REAIS[k]);
    }
    return X64SemRegra(c, "conta real", "operacao sem forma SSE");
}

/* -a = a xor mascara de sinal (16 bytes alinhados em .rdata; o endereco passa por rax porque o RIP-relativo so existe no LEA). */
bool IselNegReal(X64Ctx *c, const BirInstr *i)
{
    MOperando mascara = MMem(X64_RIP, 0, 64);

    mascara.simbolo = c->obj_sim[i->b.id];
    if (!IselCarregarReal(c, XMM_A, &i->a) || !X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RAX, 64), mascara)))
        return false;
    if (!X64Emitir(c, MI2(FX_XORPS_XM, MXmm(XMM_A), MMem(X64_RAX, 0, 128))))
        return false;
    return IselGuardarReal(c, i->dst, XMM_A);
}

/* ---- comparacao ---- */

/* troca: ucomis b,a em vez de a,b (< e <=); paridade: 1 = "e PF=0" (and com setnp), 2 = "ou PF=1" (or com setp). */
typedef struct { bool troca; X64Cond cc; uint8_t paridade; } ComparacaoReal;

/* Na ordem BCOND_FEQ..BCOND_FGE. Desordenado (NaN): ZF=PF=CF=1, entao so o != fica verdadeiro. */
static const ComparacaoReal COMPARACOES_REAIS[] = {
    {false, X64_CC_E, 1}, {false, X64_CC_NE, 2}, {true, X64_CC_A, 0}, {true, X64_CC_AE, 0}, {false, X64_CC_A, 0}, {false, X64_CC_AE, 0},
};

/* al = al and cl (setnp cl) ou al = al or cl (setp cl): a paridade do ucomis separa o NaN. */
static bool JuntarParidade(X64Ctx *c, uint8_t paridade)
{
    X64Cond cc = paridade == 1 ? X64_CC_NP : X64_CC_P;

    if (!X64Emitir(c, MICc(FX_SETCC8_R, cc, MReg(X64_RCX, 8))))
        return false;
    return X64Emitir(c, MI2(paridade == 1 ? FX_AND32_RR : FX_OR32_RR, MReg(X64_RAX, 32), MReg(X64_RCX, 32)));
}

/* xmm0 e xmm1 recebem os operandos na ordem da comparacao (trocados em < e <=). */
static bool CarregarOperandosDaComparacao(X64Ctx *c, const BirInstr *i, const ComparacaoReal *k)
{
    const BirOperando *esq = k->troca ? &i->b : &i->a;
    const BirOperando *dir = k->troca ? &i->a : &i->b;

    return IselCarregarReal(c, XMM_A, esq) && IselCarregarReal(c, XMM_B, dir);
}

/* ucomis, setcc em al e, quando a condicao depende da paridade, a juncao com cl. */
static bool EmitirTesteDaComparacao(X64Ctx *c, const BirInstr *i, const ComparacaoReal *k)
{
    if (!X64Emitir(c, MI2(i->a.classe == BC_F64 ? FX_UCOMISD_XX : FX_UCOMISS_XX, MXmm(XMM_A), MXmm(XMM_B))))
        return false;
    if (!X64Emitir(c, MICc(FX_SETCC8_R, k->cc, MReg(X64_RAX, 8))))
        return false;
    return k->paridade == 0 || JuntarParidade(c, k->paridade);
}

bool IselCmpReal(X64Ctx *c, const BirInstr *i)
{
    const ComparacaoReal *k;

    if (i->aux < BCOND_FEQ || i->aux > BCOND_FGE)
        return X64SemRegra(c, "cmp", "condicao inteira em operando real");
    k = &COMPARACOES_REAIS[i->aux - BCOND_FEQ];
    if (!CarregarOperandosDaComparacao(c, i, k))
        return false;
    if (!EmitirTesteDaComparacao(c, i, k))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

/* ---- conversoes ---- */

/* A forma do cvtsi2sd (dupla) ou cvtsi2ss, de 64 ou 32 bits. */
static uint16_t FormaDaConversaoIParaF(bool dupla, bool r64)
{
    if (dupla)
        return r64 ? FX_CVTSI2SD_XR64 : FX_CVTSI2SD_XR32;
    return r64 ? FX_CVTSI2SS_XR64 : FX_CVTSI2SS_XR32;
}

/* xmm0 = (real) rax pela forma dada; r64 = a fonte e o rax inteiro, senao o eax. */
static bool EmitirConversaoIParaF(X64Ctx *c, uint16_t forma, bool r64)
{
    return X64Emitir(c, MI2(forma, MXmm(XMM_A), MReg(X64_RAX, r64 ? 64 : 32)));
}

/* dst = (real) a. Fonte de 32 bits com sinal: cvtsi2s r32; int: r64; u32: o mov de 32 bits zera o alto e vale o r64 (BE_ZERO). */
bool IselIParaF(X64Ctx *c, const BirInstr *i)
{
    bool alarga = i->a.classe == BC_I32 && i->aux == BIR_SEM_SINAL;
    bool r64 = i->a.classe == BC_I64 || alarga;
    uint16_t forma = FormaDaConversaoIParaF(i->classe == BC_F64, r64);

    if (!IselCarregar(c, X64_RAX, &i->a, alarga ? BE_ZERO : BE_NENHUMA))
        return false;
    if (!EmitirConversaoIParaF(c, forma, r64))
        return false;
    return IselGuardarReal(c, i->dst, XMM_A);
}

/* dst = sqrt(a): sqrtss/sqrtsd xmm0, xmm0 (negativo da NaN, -0.0 da -0.0, inf da inf, como o __builtin_sqrt de C). */
bool IselRaiz(X64Ctx *c, const BirInstr *i)
{
    uint16_t forma = i->classe == BC_F64 ? FX_SQRTSD_XX : FX_SQRTSS_XX;

    if (!IselCarregarReal(c, XMM_A, &i->a) || !X64Emitir(c, MI2(forma, MXmm(XMM_A), MXmm(XMM_A))))
        return false;
    return IselGuardarReal(c, i->dst, XMM_A);
}

/* ---- vetor V128 (S2): so 4 x f32 e 2 x f64; movups para o home de 16 B e movaps entre registradores (AlocCarregar/AlocGravar) ---- */

#define XMM_SEM_REGISTRADOR (-1)

/* Numero do xmm que guarda o vetor, ou XMM_SEM_REGISTRADOR se ele vive no home. */
static int VetorEmXmm(const X64Ctx *c, uint32_t vreg)
{
    AlocOperando o = AlocOperandoDe(c->f, vreg, AlocBits(BC_V128));

    return o.em_reg ? (int)o.reg : XMM_SEM_REGISTRADOR;
}

bool IselCarregarVetor(X64Ctx *c, uint8_t xmm, uint32_t vreg)
{
    AlocOperando origem = AlocOperandoDe(c->f, vreg, AlocBits(BC_V128));

    return AlocCarregar(c, FX_MOVUPS_XM, MXmm(xmm), &origem);
}

bool IselGuardarVetor(X64Ctx *c, uint32_t vreg, uint8_t xmm)
{
    AlocOperando destino = AlocOperandoDe(c->f, vreg, AlocBits(BC_V128));

    if (destino.em_reg && (uint8_t)destino.reg == xmm)
        return true; /* o resultado ja foi calculado no registrador do dst */
    return AlocGravar(c, FX_MOVUPS_MX, &destino, MXmm(xmm));
}

/* O xmm que ja guarda o vetor do operando, ou o rascunho depois de carregar o home. */
static bool VetorParaXmm(X64Ctx *c, const BirOperando *o, uint8_t rascunho, uint8_t *xmm)
{
    int registrador = VetorEmXmm(c, o->id);

    if (registrador != XMM_SEM_REGISTRADOR) {
        *xmm = (uint8_t)registrador;
        return true;
    }
    *xmm = rascunho;
    return IselCarregarVetor(c, rascunho, o->id);
}

typedef struct { uint8_t op; uint16_t ps, pd; } ContaVetorial;

static const ContaVetorial CONTAS_VETORIAIS[] = {
    {BIR_ADD, FX_ADDPS_XX, FX_ADDPD_XX}, {BIR_SUB, FX_SUBPS_XX, FX_SUBPD_XX},
    {BIR_MUL, FX_MULPS_XX, FX_MULPD_XX}, {BIR_DIV, FX_DIVPS_XX, FX_DIVPD_XX},
};

/* O resultado sai no registrador do dst, salvo quando o dst e o 2o operando (e nao o 1o): a conta e destrutiva (op a, b) e NaN com payload diferente em a e b depende da ordem,
 * entao nao se troca os operandos (nem em ADD e MUL): usa o rascunho xmm0 e copia. Mesma conta lane a lane do escalar (xmm0 = a op b), sem FMA. */
static uint8_t XmmDoResultadoVetorial(const X64Ctx *c, const BirInstr *i)
{
    int destino = VetorEmXmm(c, i->dst);
    int primeiro = VetorEmXmm(c, i->a.id);
    int segundo = VetorEmXmm(c, i->b.id);

    if (destino == XMM_SEM_REGISTRADOR || (destino == segundo && destino != primeiro))
        return XMM_A;
    return (uint8_t)destino;
}

bool IselContaVetorial(X64Ctx *c, const BirInstr *i)
{
    uint8_t alvo = XmmDoResultadoVetorial(c, i), segundo;
    const ContaVetorial *conta = NULL;

    for (uint32_t k = 0; k < sizeof CONTAS_VETORIAIS / sizeof CONTAS_VETORIAIS[0]; k++) {
        if (CONTAS_VETORIAIS[k].op == i->op)
            conta = &CONTAS_VETORIAIS[k];
    }
    if (conta == NULL)
        return X64SemRegra(c, "conta vetorial", "operacao sem forma SSE empacotada");
    if (VetorEmXmm(c, i->a.id) != (int)alvo && !IselCarregarVetor(c, alvo, i->a.id))
        return false;
    if (!VetorParaXmm(c, &i->b, XMM_B, &segundo))
        return false;
    if (!X64Emitir(c, MI2(i->aux == BVEC_2XF64 ? conta->pd : conta->ps, MXmm(alvo), MXmm(segundo))))
        return false;
    return IselGuardarVetor(c, i->dst, alvo);
}

/* dst = a difundido nas lanes: o escalar vai ao xmm do resultado (movss/movsd do home ou movaps), depois shufps xmm,xmm,0 (f32) ou unpcklpd xmm,xmm (f64). */
bool IselSplat(X64Ctx *c, const BirInstr *i)
{
    int destino = VetorEmXmm(c, i->dst);
    uint8_t alvo = destino == XMM_SEM_REGISTRADOR ? XMM_A : (uint8_t)destino;
    MInstr difundir = i->aux == BVEC_2XF64 ? MI2(FX_UNPCKLPD_XX, MXmm(alvo), MXmm(alvo)) : MI3(FX_SHUFPS_XXI, MXmm(alvo), MXmm(alvo), MImed(0));

    if (!IselCarregarReal(c, alvo, &i->a) || !X64Emitir(c, difundir))
        return false;
    return IselGuardarVetor(c, i->dst, alvo);
}

bool IselCopiaVetor(X64Ctx *c, const BirInstr *i)
{
    int destino = VetorEmXmm(c, i->dst);
    uint8_t origem;

    if (!VetorParaXmm(c, &i->a, destino == XMM_SEM_REGISTRADOR ? XMM_A : (uint8_t)destino, &origem))
        return false;
    return IselGuardarVetor(c, i->dst, origem);
}

bool IselCarregaVetor(X64Ctx *c, const BirInstr *i)
{
    int destino = VetorEmXmm(c, i->dst);
    uint8_t alvo = destino == XMM_SEM_REGISTRADOR ? XMM_A : (uint8_t)destino;

    if (!IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA))
        return false;
    if (!X64Emitir(c, MI2(FX_MOVUPS_XM, MXmm(alvo), MMem(X64_RCX, i->desloc, AlocBits(BC_V128)))))
        return false;
    return IselGuardarVetor(c, i->dst, alvo);
}

bool IselGravaVetor(X64Ctx *c, const BirInstr *i)
{
    uint8_t valor;

    if (!IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA) || !VetorParaXmm(c, &i->b, XMM_A, &valor))
        return false;
    return X64Emitir(c, MI2(FX_MOVUPS_MX, MMem(X64_RCX, i->desloc, AlocBits(BC_V128)), MXmm(valor)));
}

/* F32 -> F64 por cvtss2sd e F64 -> F32 por cvtsd2ss (o que nao cabe vira inf, como o cast do C). */
bool IselFParaF(X64Ctx *c, const BirInstr *i)
{
    uint16_t forma = i->classe == BC_F64 ? FX_CVTSS2SD_XX : FX_CVTSD2SS_XX;

    if (!IselCarregarReal(c, XMM_A, &i->a) || !X64Emitir(c, MI2(forma, MXmm(XMM_A), MXmm(XMM_A))))
        return false;
    return IselGuardarReal(c, i->dst, XMM_A);
}
