/* x64_isel_otim_fluxo.c - selecao pos-alocador (M7a F5): fusao CMP -> DESVIA (cmp + jcc, sem setcc/test), fallthrough com inversao do jcc e DESVIA direto no registrador.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - R5 (SIMD): IselSeleciona (SELECIONA: cmp + cmovcc em inteiro, minss/maxss em real); IselOtimFlagsCmpInteiro passa a ser publica.
 *            0.1.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.1.0 - M7a F5: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F4> --release-muda
 *
 * A fusao so vale com o CMP imediatamente antes do DESVIA que o consome e o bool com uma definicao e um uso (nada mais le o 0/1). As flags nascem no cmp e morrem no jcc da
 * instrucao seguinte da maquina: nada se intercala. Real: so as condicoes sem a paridade (< <= > >=); o NaN deixa CF=1, entao ja/jae nao saltam e a inversao (jbe/jb) salta,
 * como a comparacao falsa pede.
 */
#include "nativo/x64/x64_isel_otim.h"

static bool ClasseDeConta(uint8_t classe)
{
    return classe == BC_I32 || classe == BC_I64;
}

/* cmp a, b de I32/I64 com os operandos em registrador vivo (ou rax/rcx); com b = 0 vira test a,a (mesmas flags ZF/SF/PF, CF = OF = 0). */
static bool EmitirCmpInteiro(X64Ctx *c, const BirInstr *i)
{
    uint8_t larg = IselIndiceLargura(i->a.classe);
    uint8_t bits = (uint8_t)(32u << larg);
    const FormasAritmeticas *formas = &FORMAS_ARIT[BIR_CMP];
    X64Reg esq, dir;
    int64_t imm;
    bool ok;

    esq = IselOtimFonte(c, &i->a, X64_RAX, &ok);
    if (!ok)
        return false;
    if (IselOtimImediato(&i->b, &imm)) {
        if (imm == 0)
            return X64Emitir(c, MI2(larg ? FX_TEST64_RR : FX_TEST32_RR, MReg(esq, bits), MReg(esq, bits)));
        return X64Emitir(c, MI2(formas->ri[larg], MReg(esq, bits), MImed(imm)));
    }
    dir = IselOtimFonte(c, &i->b, X64_RCX, &ok);
    return ok && X64Emitir(c, MI2(formas->rr[larg], MReg(esq, bits), MReg(dir, bits)));
}

/* Salta para alvo[0] se cc, senao para alvo[1]; o bloco seguinte na ordem de emissao vira fallthrough (jcc invertido: o cc par/impar do x86 difere no bit 0). */
static bool EmitirSaltoCondicional(X64Ctx *c, const BirInstr *desvia, X64Cond cc)
{
    uint32_t proximo = QuadroProximoBloco(c);

    if (desvia->alvo[0] == proximo && desvia->alvo[1] != proximo)
        return X64Emitir(c, MICc(FX_JCC_REL32, (X64Cond)(cc ^ 1), MBloco(desvia->alvo[1])));
    if (!X64Emitir(c, MICc(FX_JCC_REL32, cc, MBloco(desvia->alvo[0]))))
        return false;
    return IselSalto(c, desvia->alvo[1]);
}

/* As flags do CMP de inteiro (cc em *cc); 0 sem emitir se a classe ou a condicao nao e deste caminho. */
int32_t IselOtimFlagsCmpInteiro(X64Ctx *c, const BirInstr *i, X64Cond *cc)
{
    if (!ClasseDeConta(i->a.classe) || i->b.classe != i->a.classe || i->aux >= 10u)
        return 0;
    if (!EmitirCmpInteiro(c, i))
        return -1;
    *cc = CC_DA_COND[i->aux];
    return 1;
}

int32_t IselOtimComparaDesvia(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *cmp = &bloco->instrs[k];
    const BirInstr *desvia = &bloco->instrs[k + 1u];
    X64Cond cc = X64_CC_E;
    int32_t r;

    if (desvia->op != BIR_DESVIA || desvia->a.tipo != BO_VREG || desvia->a.id != cmp->dst || !IselOtimTempUnico(c, cmp->dst))
        return 0;
    r = BirClasseEhReal(cmp->a.classe) ? IselOtimCmpReal(c, cmp, &cc) : IselOtimFlagsCmpInteiro(c, cmp, &cc);
    if (r <= 0)
        return r;
    c->instr = desvia;
    return EmitirSaltoCondicional(c, desvia, cc) ? 2 : -1;
}

/* CMP que guarda o 0/1: cmp com registradores vivos, setcc al, guarda. */
int32_t IselOtimCompara(X64Ctx *c, const BirInstr *i)
{
    X64Cond cc;
    int32_t r = IselOtimFlagsCmpInteiro(c, i, &cc);

    if (r <= 0)
        return r;
    if (!X64Emitir(c, MICc(FX_SETCC8_R, cc, MReg(X64_RAX, 8))))
        return -1;
    return IselGuardar(c, i->dst, X64_RAX) ? 1 : -1;
}

/* DESVIA sobre bool que mora em registrador: test r,r em vez de movzx + test. */
int32_t IselOtimDesvia(X64Ctx *c, const BirInstr *i)
{
    int r = IselOtimRegInt(c, &i->a);

    if (r < 0)
        return 0;
    if (!X64Emitir(c, MI2(FX_TEST32_RR, MReg((X64Reg)r, 32), MReg((X64Reg)r, 32))))
        return -1;
    return EmitirSaltoCondicional(c, i, X64_CC_NE) ? 1 : -1;
}

/* ---- SELECIONA (R5) ---- */

/* O vreg do dst e tambem operando (a, b ou da comparacao)? Entao o resultado nao pode ser escrito antes de tudo ser lido: calcula em rax e guarda. */
static bool DestinoEhOperando(const BirInstr *i)
{
    const BirOperando *op[4] = {&i->a, &i->b, &i->args[0], &i->args[1]};

    for (uint32_t k = 0; k < 4; k++) {
        if (op[k]->tipo == BO_VREG && op[k]->id == i->dst)
            return true;
    }
    return false;
}

/* As flags de "args[0] aux args[1]": o caminho de I32/I64 do desvio fundido; I8 e I16 (sinal pelo alargamento) vao pela conta comum cmp. */
static bool FlagsDaSelecao(X64Ctx *c, const BirInstr *i, X64Cond *cc)
{
    BirInstr cmp = {.op = BIR_CMP, .classe = BC_I8, .aux = i->aux, .dst = BIR_SEM_VREG, .a = i->args[0], .b = i->args[1]};
    bool com_sinal = i->aux >= BCOND_LT_S && i->aux <= BCOND_GE_S;
    int32_t r = IselOtimFlagsCmpInteiro(c, &cmp, cc);

    if (r != 0)
        return r > 0;
    if (i->aux >= sizeof CC_DA_COND / sizeof CC_DA_COND[0] || !IselOperacao(c, &cmp, BIR_CMP, IselExtensaoDaConta(cmp.a.classe, com_sinal)))
        return false;
    *cc = CC_DA_COND[i->aux];
    return true;
}

/* dst = cond ? a : b em inteiro: cmp, dst = b (mov nao mexe nas flags), cmovcc dst, a. rax e rcx sao rascunho (a comparacao ja acabou quando eles recebem b e a). */
static bool SelecionaInteiro(X64Ctx *c, const BirInstr *i)
{
    uint8_t bits = (uint8_t)(32u << IselIndiceLargura(i->classe));
    int casa = IselOtimRegDestino(c, i->dst);
    bool direto = casa >= 0 && !DestinoEhOperando(i);
    X64Reg dst = direto ? (X64Reg)casa : X64_RAX, origem;
    X64Cond cc;
    MInstr cmov;
    bool ok;

    if (!FlagsDaSelecao(c, i, &cc) || !IselOtimCarregarEm(c, dst, &i->b, BE_NENHUMA))
        return false;
    origem = IselOtimFonte(c, &i->a, X64_RCX, &ok);
    if (!ok)
        return false;
    cmov = MI2(bits == 64 ? FX_CMOV64_RR : FX_CMOV32_RR, MReg(dst, bits), MReg(origem, bits));
    cmov.cond = cc;
    if (!X64Emitir(c, cmov))
        return false;
    return direto || IselGuardar(c, i->dst, X64_RAX);
}

/* SELECIONA: vale tambem sem o passe `isel` (usa so o alocador e os rascunhos rax/rcx/xmm0/xmm1). */
bool IselSeleciona(X64Ctx *c, const BirInstr *i)
{
    return BirClasseEhReal(i->classe) ? IselOtimSelecionaReal(c, i) : SelecionaInteiro(c, i);
}
