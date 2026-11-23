/* x64_isel_otim.c - selecao pos-alocador (M7a F5, passe `isel`): o resultado e calculado no registrador do proprio dst, os operandos vivos em registrador entram direto
 * (sem passar por rax/rcx, sem mov r,r para o mesmo registrador), constante pequena vira lea/shl/imul r,r,imm e a divisao sem sinal por 2^k vira shr/and.
 * Versao: 0.2.2
 * Changelog: 0.2.2 - S2 (D0703): V128 em IselOtimTentar (so CARREGA/GRAVA, DespacharVetor; o resto vai para a selecao comum) e IselOtimRegXmmDestino por BirClasseEhXmm.
 *            0.2.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.2.0 - M7b F1: DIV/REM de inteiro por imediato vao primeiro para IselOtimDivConst (x64_isel_otim_div.c) quando a funcao tem div_const; IMED_COLIDE_* foram para o .h.
 *            0.1.0 - M7a F5: primeira versao (aritmetica, *_OV de I32/I64, mover/estender, NEG/NOT, SHL/SHR, DIV/REM sem sinal por 2^k, contagem de usos).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F4> --release-muda
 *
 * Tudo aqui so vale com BirFuncao.isel_otim (release); sem ele X64Selecionar segue o caminho de x64_isel*.c byte a byte. Os *_OV nao calculam em registrador que seja
 * operando (o bloco frio os le); a posicao do desvio para o frio nao muda (R4): a conta termina em jcc, como na selecao comum.
 */
#include "nativo/x64/x64_isel_otim.h"

/* ---- auxiliares compartilhados ---- */

int IselOtimRegDestino(const X64Ctx *c, uint32_t vreg)
{
    uint8_t classe;

    if (c->f->reg_vreg == NULL || vreg >= c->f->nvregs || c->f->reg_vreg[vreg] == BIR_SEM_REG)
        return -1;
    classe = c->f->classe_vreg[vreg];
    return classe >= BC_I8 && classe <= BC_I64 ? c->f->reg_vreg[vreg] : -1;
}

int IselOtimRegInt(const X64Ctx *c, const BirOperando *o)
{
    return o->tipo == BO_VREG ? IselOtimRegDestino(c, o->id) : -1;
}

int IselOtimRegXmmDestino(const X64Ctx *c, uint32_t vreg)
{
    uint8_t classe;

    if (c->f->reg_vreg == NULL || vreg >= c->f->nvregs || c->f->reg_vreg[vreg] == BIR_SEM_REG)
        return -1;
    classe = c->f->classe_vreg[vreg];
    return BirClasseEhXmm(classe) ? c->f->reg_vreg[vreg] : -1; /* F32, F64 e o vetor V128 (S2) */
}

int IselOtimRegXmm(const X64Ctx *c, const BirOperando *o)
{
    return o->tipo == BO_VREG ? IselOtimRegXmmDestino(c, o->id) : -1;
}

bool IselOtimTempUnico(const X64Ctx *c, uint32_t vreg)
{
    return c->usos != NULL && vreg < c->f->nvregs && c->usos[vreg] == 1 && c->defs[vreg] == 1;
}

X64Reg IselOtimFonte(X64Ctx *c, const BirOperando *o, X64Reg rascunho, bool *ok)
{
    int r = IselOtimRegInt(c, o);

    if (r >= 0) {
        *ok = true;
        return (X64Reg)r;
    }
    *ok = IselCarregar(c, rascunho, o, BE_NENHUMA);
    return rascunho;
}

bool IselOtimCarregarEm(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    if (ext != BE_SINAL && IselOtimRegInt(c, o) == (int)reg)
        return true; /* o registrador ja guarda o valor zero-extendido */
    return IselCarregar(c, reg, o, ext);
}

bool IselOtimEncodavel(MOperando *mem)
{
    X64Reg troca;

    if (mem->escala == 0 || mem->desloc != 0 || (mem->base & 7) != 5)
        return true;
    if (mem->escala != 1 || (mem->indice & 7) == 5)
        return false;
    troca = mem->base; /* [rbp+idx*1] = [idx+rbp*1]: o r13/rbp passa a indice, que o SIB aceita sem deslocamento */
    mem->base = mem->indice;
    mem->indice = troca;
    return true;
}

bool IselOtimImediato(const BirOperando *o, int64_t *valor)
{
    if (o->tipo != BO_IMED || (o->classe != BC_I32 && o->classe != BC_I64))
        return false;
    *valor = o->classe == BC_I32 ? (int64_t)(int32_t)o->imed : o->imed;
    return *valor >= INT32_MIN && *valor <= INT32_MAX;
}

bool IselOtimPotenciaDe2(const BirOperando *o, uint8_t *k)
{
    uint64_t v;

    if (o->tipo != BO_IMED || (o->classe != BC_I32 && o->classe != BC_I64))
        return false;
    v = o->classe == BC_I32 ? (uint64_t)(uint32_t)o->imed : (uint64_t)o->imed;
    if (v == 0 || (v & (v - 1u)) != 0)
        return false;
    *k = (uint8_t)__builtin_ctzll(v);
    return true;
}

static void ContarOperando(const BirOperando *o, uint8_t *usos, uint32_t nvregs)
{
    if (o->tipo == BO_VREG && o->id < nvregs && usos[o->id] < UINT8_MAX)
        usos[o->id]++;
}

void IselOtimContarUsos(const BirFuncao *f, uint8_t *usos, uint8_t *defs)
{
    for (uint32_t v = 0; v < f->nvregs; v++) {
        usos[v] = 0;
        defs[v] = v < f->nparams ? 1u : 0u; /* o parametro nasce na entrada */
    }
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++) {
            const BirInstr *i = &f->blocos[b].instrs[k];

            if (i->dst != BIR_SEM_VREG && i->dst < f->nvregs && defs[i->dst] < UINT8_MAX)
                defs[i->dst]++;
            ContarOperando(&i->a, usos, f->nvregs);
            ContarOperando(&i->b, usos, f->nvregs);
            for (uint32_t n = 0; n < i->nargs; n++)
                ContarOperando(&i->args[n], usos, f->nvregs);
        }
    }
}

static bool ClasseDeConta(uint8_t classe)
{
    return classe == BC_I32 || classe == BC_I64;
}

static bool EhVreg(const BirOperando *o)
{
    return o->tipo == BO_VREG;
}

static bool MesmoVreg(const BirOperando *x, const BirOperando *y)
{
    return EhVreg(x) && EhVreg(y) && x->id == y->id;
}

static bool Comutativa(uint8_t op)
{
    return op == BIR_ADD || op == BIR_MUL || op == BIR_AND || op == BIR_OR || op == BIR_XOR;
}

/* Fecha a conta: calculada em rax (dst fora de registrador ou em conflito) vai para o dst; calculada no proprio registrador do dst nada resta. */
static int32_t Fechar(X64Ctx *c, uint32_t dst, X64Reg alvo, int32_t r)
{
    if (r <= 0 || alvo != X64_RAX)
        return r;
    return IselGuardar(c, dst, X64_RAX) ? 1 : -1;
}

/* ---- aritmetica ---- */

/* alvo = onde a conta e feita; a e b ja na ordem em que entram (o imediato fica em b nas contas comutativas). */
typedef struct { X64Reg alvo; const BirOperando *a, *b; } ContaOtim;

static ContaOtim PrepararConta(const X64Ctx *c, const BirInstr *i)
{
    ContaOtim k = {X64_RAX, &i->a, &i->b};
    int destino = IselOtimRegDestino(c, i->dst);

    if (Comutativa(i->op) && i->a.tipo == BO_IMED && i->b.tipo != BO_IMED) {
        k.a = &i->b;
        k.b = &i->a;
    }
    if (destino < 0)
        return k;
    k.alvo = (X64Reg)destino;
    if (IselOtimRegInt(c, k.b) != destino || MesmoVreg(k.a, k.b))
        return k;
    if (!Comutativa(i->op) || k.a->tipo == BO_IMED) {
        k.alvo = X64_RAX; /* b mora no registrador do dst: calcula em rax e guarda */
        return k;
    }
    {
        const BirOperando *no_alvo = k.b; /* comutativa: o operando que mora no alvo passa a ser o primeiro e a conta entra em lugar */

        k.b = k.a;
        k.a = no_alvo;
    }
    return k;
}

/* ADD/SUB de 64 bits sem flags: lea alvo,[ra+rb] ou [ra+imm] quando a conta nao e em lugar (alvo != ra). */
static int32_t TentarLea(X64Ctx *c, const BirInstr *i, const ContaOtim *k)
{
    int ra = IselOtimRegInt(c, k->a), rb = IselOtimRegInt(c, k->b);
    int64_t imm;
    MOperando mem;

    if ((i->op != BIR_ADD && i->op != BIR_SUB) || i->classe != BC_I64 || ra < 0 || ra == (int)k->alvo)
        return 0;
    if (IselOtimImediato(k->b, &imm) && !(i->op == BIR_SUB && imm == INT32_MIN))
        mem = MMem((X64Reg)ra, (int32_t)(i->op == BIR_SUB ? -imm : imm), 64);
    else if (i->op == BIR_ADD && rb >= 0 && rb != (int)k->alvo) {
        mem = MMem((X64Reg)ra, 0, 64);
        mem.indice = (X64Reg)rb;
        mem.escala = 1;
    } else
        return 0;
    if (!IselOtimEncodavel(&mem))
        return 0;
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg(k->alvo, 64), mem)) ? 1 : -1;
}

/* x * 2^pot = mov + shl. */
static int32_t MulPorPotenciaDe2(X64Ctx *c, const BirInstr *i, const ContaOtim *k, uint8_t pot)
{
    uint8_t larg = IselIndiceLargura(i->classe);

    if (!IselOtimCarregarEm(c, k->alvo, k->a, BE_NENHUMA))
        return -1;
    return pot == 0 || X64Emitir(c, MI2(larg ? FX_SHL64_RI : FX_SHL32_RI, MReg(k->alvo, (uint8_t)(32u << larg)), MImed(pot))) ? 1 : -1;
}

/* x * 3, 5 ou 9 de 64 bits = lea alvo,[x+x*2|4|8]; 0 se a forma nao serve. */
static int32_t MulPorLea(X64Ctx *c, const ContaOtim *k, int64_t v)
{
    int ra = IselOtimRegInt(c, k->a);
    MOperando mem;

    if (ra < 0 || (v != 3 && v != 5 && v != 9))
        return 0;
    mem = MMem((X64Reg)ra, 0, 64);
    mem.indice = (X64Reg)ra;
    mem.escala = (uint8_t)(v - 1);
    if (!IselOtimEncodavel(&mem))
        return 0;
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg(k->alvo, 64), mem)) ? 1 : -1;
}

/* imul alvo, a, imm32 (o imediato fora de 0x204..0x207: veja IMED_COLIDE_MIN). */
static int32_t MulPorImediato(X64Ctx *c, const BirInstr *i, const ContaOtim *k, int64_t v)
{
    uint8_t larg = IselIndiceLargura(i->classe);
    uint8_t bits = (uint8_t)(32u << larg);
    X64Reg fonte;
    bool ok;

    if (v >= IMED_COLIDE_MIN && v <= IMED_COLIDE_MAX)
        return 0;
    fonte = IselOtimFonte(c, k->a, k->alvo, &ok);
    if (!ok)
        return -1;
    return X64Emitir(c, MI3(larg ? FX_IMUL64_RRI : FX_IMUL32_RRI, MReg(k->alvo, bits), MReg(fonte, bits), MImed(v))) ? 1 : -1;
}

/* MUL por constante: 0 = xor, 2^k = shl, 3/5/9 (64 bits) = lea, resto = imul r,r,imm32. */
static int32_t MulPorConstante(X64Ctx *c, const BirInstr *i, const ContaOtim *k, int64_t v)
{
    uint8_t pot;
    int32_t r;

    if (v == 0)
        return X64Emitir(c, MI2(FX_XOR32_RR, MReg(k->alvo, 32), MReg(k->alvo, 32))) ? 1 : -1;
    if (v > 0 && IselOtimPotenciaDe2(k->b, &pot))
        return MulPorPotenciaDe2(c, i, k, pot);
    r = i->classe == BC_I64 ? MulPorLea(c, k, v) : 0;
    return r != 0 ? r : MulPorImediato(c, i, k, v);
}

static int32_t TentarMulImediato(X64Ctx *c, const BirInstr *i, const ContaOtim *k)
{
    int64_t v;

    if (i->op != BIR_MUL || !IselOtimImediato(k->b, &v))
        return 0;
    return MulPorConstante(c, i, k, v);
}

/* alvo = a; alvo = alvo op b, com b imediato (forma ri), registrador vivo (direto) ou carregado em rcx. */
static int32_t ContaGeral(X64Ctx *c, const BirInstr *i, const ContaOtim *k)
{
    const FormasAritmeticas *formas = &FORMAS_ARIT[i->op];
    uint8_t larg = IselIndiceLargura(i->classe);
    uint8_t bits = (uint8_t)(32u << larg);
    int64_t imm;
    X64Reg fonte;
    bool ok;

    if (!IselOtimCarregarEm(c, k->alvo, k->a, BE_NENHUMA))
        return -1;
    if (formas->ri[larg] != FX_QTD && IselOtimImediato(k->b, &imm))
        return X64Emitir(c, MI2(formas->ri[larg], MReg(k->alvo, bits), MImed(imm))) ? 1 : -1;
    fonte = IselOtimFonte(c, k->b, X64_RCX, &ok);
    if (!ok)
        return -1;
    return X64Emitir(c, MI2(formas->rr[larg], MReg(k->alvo, bits), MReg(fonte, bits))) ? 1 : -1;
}

int32_t IselOtimAritmetica(X64Ctx *c, const BirInstr *i)
{
    ContaOtim k;
    int32_t r;

    if (!ClasseDeConta(i->classe) || i->a.classe != i->classe || i->b.classe != i->classe)
        return 0;
    k = PrepararConta(c, i);
    r = TentarLea(c, i, &k);
    if (r == 0)
        r = TentarMulImediato(c, i, &k);
    if (r == 0)
        r = ContaGeral(c, i, &k);
    return Fechar(c, i->dst, k.alvo, r);
}

/* ---- *_OV de I32 e I64 ---- */

static X64Cond CondicaoDoOv(const BirInstr *i, uint8_t base)
{
    return (i->aux == BIR_SINAL || base == BIR_MUL) ? X64_CC_O : X64_CC_B;
}

/* MUL_OV com sinal: imul r,r,imm (OF acende no estouro) ou imul r,r. */
static int32_t MulComSinal(X64Ctx *c, const BirInstr *i, X64Reg alvo, const ContaOtim *k)
{
    uint8_t larg = IselIndiceLargura(i->classe);
    uint8_t bits = (uint8_t)(32u << larg);
    int64_t v;
    X64Reg fonte;
    bool ok;

    if (IselOtimImediato(k->b, &v) && !(v >= IMED_COLIDE_MIN && v <= IMED_COLIDE_MAX)) {
        fonte = IselOtimFonte(c, k->a, alvo, &ok);
        if (!ok)
            return -1;
        return X64Emitir(c, MI3(larg ? FX_IMUL64_RRI : FX_IMUL32_RRI, MReg(alvo, bits), MReg(fonte, bits), MImed(v))) ? 1 : -1;
    }
    if (!IselOtimCarregarEm(c, alvo, k->a, BE_NENHUMA))
        return -1;
    fonte = IselOtimFonte(c, k->b, X64_RCX, &ok);
    if (!ok)
        return -1;
    return X64Emitir(c, MI2(larg ? FX_IMUL64_RR : FX_IMUL32_RR, MReg(alvo, bits), MReg(fonte, bits))) ? 1 : -1;
}

static int32_t ContaDoOv(X64Ctx *c, const BirInstr *i, uint8_t base, const ContaOtim *k)
{
    if (base == BIR_MUL)
        return MulComSinal(c, i, k->alvo, k);
    return ContaGeral(c, &(BirInstr){.op = base, .classe = i->classe}, k);
}

/* dst em registrador que nao e de operando: calcula direto nele e salta ao frio no estouro; senao 0 (a selecao comum guarda so depois do desvio). */
int32_t IselOtimOverflow(X64Ctx *c, const BirInstr *i)
{
    uint8_t base = i->op == BIR_ADD_OV ? BIR_ADD : (i->op == BIR_SUB_OV ? BIR_SUB : BIR_MUL);
    int destino = IselOtimRegDestino(c, i->dst);
    ContaOtim k = {X64_RAX, &i->a, &i->b};
    int32_t r;

    if (!ClasseDeConta(i->classe) || i->a.classe != i->classe || destino < 0)
        return 0;
    if (base == BIR_MUL && i->aux == BIR_SEM_SINAL)
        return 0;
    if (destino == IselOtimRegInt(c, &i->a) || destino == IselOtimRegInt(c, &i->b))
        return 0;
    if (base != BIR_SUB && i->a.tipo == BO_IMED && i->b.tipo != BO_IMED) {
        k.a = &i->b;
        k.b = &i->a;
    }
    k.alvo = (X64Reg)destino;
    r = ContaDoOv(c, i, base, &k);
    if (r <= 0)
        return r;
    return X64Emitir(c, MICc(FX_JCC_REL32, CondicaoDoOv(i, base), MBloco(i->alvo[0]))) ? 1 : -1;
}

/* ---- mover e estender ---- */

/* CONST/COPIA/TRUNC de imediato da classe do dst: xor para 0, mov r32 (zera o alto) ate 2^32-1, mov r64 no resto. */
static int32_t MoverImediato(X64Ctx *c, const BirInstr *i, X64Reg d, uint8_t classe)
{
    uint64_t v = classe == BC_I32 ? (uint64_t)(uint32_t)i->a.imed : (uint64_t)i->a.imed;

    if (i->a.classe != classe || (i->op != BIR_CONST && i->op != BIR_COPIA && i->op != BIR_TRUNC))
        return 0;
    if (v == 0)
        return X64Emitir(c, MI2(FX_XOR32_RR, MReg(d, 32), MReg(d, 32))) ? 1 : -1;
    if (v <= UINT32_MAX)
        return X64Emitir(c, MI2(FX_MOV32_RI, MReg(d, 32), MImed((int32_t)(uint32_t)v))) ? 1 : -1;
    return X64Emitir(c, MI2(FX_MOV64_RI, MReg(d, 64), MImed(i->a.imed))) ? 1 : -1;
}

/* TRUNC de I64 para I32: os 32 bits baixos (mov r32 zera o alto, invariante do I32 em registrador). */
static int32_t TruncarPara32(X64Ctx *c, const BirInstr *i, X64Reg d)
{
    BirOperando baixo = i->a;
    int ra = IselOtimRegInt(c, &i->a);

    if (ra >= 0)
        return X64Emitir(c, MI2(FX_MOV32_RR, MReg(d, 32), MReg((X64Reg)ra, 32))) ? 1 : -1;
    baixo.classe = BC_I32;
    return IselCarregar(c, d, &baixo, BE_NENHUMA) ? 1 : -1;
}

static int32_t EstenderComSinal(X64Ctx *c, const BirInstr *i, X64Reg d)
{
    int ra = IselOtimRegInt(c, &i->a);

    if (i->a.classe == BC_I32 && ra >= 0)
        return X64Emitir(c, MI2(FX_MOVSXD64_RR32, MReg(d, 64), MReg((X64Reg)ra, 32))) ? 1 : -1;
    return IselOtimCarregarEm(c, d, &i->a, BE_SINAL) ? 1 : -1;
}

static int32_t MoverVreg(X64Ctx *c, const BirInstr *i, X64Reg d, uint8_t classe)
{
    uint8_t origem = i->a.classe;

    if (i->op == BIR_EXT_S)
        return classe == BC_I64 && origem >= BC_I8 && origem <= BC_I32 ? EstenderComSinal(c, i, d) : 0;
    if (i->op == BIR_EXT_Z) {
        if (origem < BC_I8 || origem >= classe)
            return 0;
        return IselOtimCarregarEm(c, d, &i->a, BE_ZERO) ? 1 : -1;
    }
    if (i->op == BIR_TRUNC && origem == BC_I64 && classe == BC_I32)
        return TruncarPara32(c, i, d);
    if (origem != classe)
        return 0;
    return IselOtimCarregarEm(c, d, &i->a, BE_NENHUMA) ? 1 : -1;
}

int32_t IselOtimMover(X64Ctx *c, const BirInstr *i)
{
    int destino = IselOtimRegDestino(c, i->dst);
    uint8_t classe = c->f->classe_vreg[i->dst];

    if (destino < 0 || !ClasseDeConta(classe))
        return 0;
    if (i->a.tipo == BO_IMED)
        return MoverImediato(c, i, (X64Reg)destino, classe);
    return i->a.tipo == BO_VREG ? MoverVreg(c, i, (X64Reg)destino, classe) : 0;
}

/* ---- NEG e NOT ---- */

int32_t IselOtimUnaria(X64Ctx *c, const BirInstr *i)
{
    uint8_t larg = IselIndiceLargura(i->classe);
    int destino = IselOtimRegDestino(c, i->dst);
    X64Reg alvo = destino >= 0 ? (X64Reg)destino : X64_RAX;
    uint16_t forma;

    if (!ClasseDeConta(i->classe) || i->a.classe != i->classe)
        return 0;
    if (!IselOtimCarregarEm(c, alvo, &i->a, BE_NENHUMA))
        return -1;
    forma = i->op == BIR_NEG ? (larg ? FX_NEG64_R : FX_NEG32_R) : (larg ? FX_NOT64_R : FX_NOT32_R);
    if (!X64Emitir(c, MI1(forma, MReg(alvo, (uint8_t)(32u << larg)))))
        return -1;
    return Fechar(c, i->dst, alvo, 1);
}

/* ---- SHL, SHR e DIV/REM sem sinal por 2^k ---- */

/* [shr][largura]: por cl e por imm8. */
static const uint16_t DESLOCA_CL[2][2] = {{FX_SHL32_RCL, FX_SHL64_RCL}, {FX_SHR32_RCL, FX_SHR64_RCL}};
static const uint16_t DESLOCA_IMM[2][2] = {{FX_SHL32_RI, FX_SHL64_RI}, {FX_SHR32_RI, FX_SHR64_RI}};

static int32_t Deslocar(X64Ctx *c, const BirInstr *i, X64Reg alvo)
{
    uint8_t sentido = i->op == BIR_SHR;
    uint8_t larg = IselIndiceLargura(i->classe);
    MOperando reg = MReg(alvo, (uint8_t)(32u << larg));

    if (i->b.tipo == BO_IMED) {
        if (!IselOtimCarregarEm(c, alvo, &i->a, BE_NENHUMA))
            return -1;
        return X64Emitir(c, MI2(DESLOCA_IMM[sentido][larg], reg, MImed(i->b.imed))) ? 1 : -1;
    }
    if (!IselOtimCarregarEm(c, X64_RCX, &i->b, BE_NENHUMA) || !IselOtimCarregarEm(c, alvo, &i->a, BE_NENHUMA))
        return -1;
    return X64Emitir(c, MI2(DESLOCA_CL[sentido][larg], reg, MReg(X64_RCX, 8))) ? 1 : -1;
}

/* a / 2^k = a >> k e a % 2^k = a & (2^k - 1), so sem sinal (com sinal pede SAR: M7b). */
static int32_t DividirPorPotencia(X64Ctx *c, const BirInstr *i, X64Reg alvo, uint8_t k)
{
    uint8_t larg = IselIndiceLargura(i->classe);
    uint8_t bits = (uint8_t)(32u << larg);
    int64_t mascara = (int64_t)((1ull << k) - 1u);

    if (i->op == BIR_REM && mascara == 0)
        return X64Emitir(c, MI2(FX_XOR32_RR, MReg(alvo, 32), MReg(alvo, 32))) ? 1 : -1;
    if (i->op == BIR_REM && mascara > INT32_MAX)
        return 0;
    if (!IselOtimCarregarEm(c, alvo, &i->a, BE_NENHUMA))
        return -1;
    if (i->op == BIR_REM)
        return X64Emitir(c, MI2(FORMAS_ARIT[BIR_AND].ri[larg], MReg(alvo, bits), MImed(mascara))) ? 1 : -1;
    return k == 0 || X64Emitir(c, MI2(larg ? FX_SHR64_RI : FX_SHR32_RI, MReg(alvo, bits), MImed(k))) ? 1 : -1;
}

int32_t IselOtimDeslocaDivide(X64Ctx *c, const BirInstr *i)
{
    int destino = IselOtimRegDestino(c, i->dst);
    X64Reg alvo = destino >= 0 ? (X64Reg)destino : X64_RAX;
    uint8_t k;
    int32_t r;

    if (!ClasseDeConta(i->classe) || i->a.classe != i->classe)
        return 0;
    if (i->op == BIR_SHL || i->op == BIR_SHR)
        r = Deslocar(c, i, alvo);
    else if (i->aux == BIR_SEM_SINAL && IselOtimPotenciaDe2(&i->b, &k))
        r = DividirPorPotencia(c, i, alvo, k);
    else
        return 0;
    return Fechar(c, i->dst, alvo, r);
}

/* Passe `isel` (tabela de passes): liga a selecao pos-alocador na funcao; nao muda a BIR. */
void X64IselMarcar(const BirPrograma *programa, BirFuncao *funcao)
{
    (void)programa;
    funcao->isel_otim = true;
}

/* ---- despacho ---- */

/* DIV/REM de inteiro: por imediato e com o passe divconst, mul magico e deslocamentos; o que ele nao toma (e a divisao sem sinal por 2^k de 32 e 64 bits) segue a conta daqui. */
static int32_t DividirInteiro(X64Ctx *c, const BirInstr *i)
{
    int32_t r = c->f->div_const && i->b.tipo == BO_IMED ? IselOtimDivConst(c, i) : 0;

    return r != 0 ? r : IselOtimDeslocaDivide(c, i);
}

/* Instrucao de valor que calcula o proprio dst (sem olhar as vizinhas); i pode ser uma copia da BIR com o dst trocado (TentarDestinoFundido). */
static int32_t DespacharUma(X64Ctx *c, const BirInstr *i)
{
    bool real = BirClasseEhReal(i->classe);

    switch (i->op) {
    case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_AND: case BIR_OR: case BIR_XOR: return real ? IselOtimReal(c, i) : IselOtimAritmetica(c, i);
    case BIR_DIV: return real ? IselOtimReal(c, i) : DividirInteiro(c, i);
    case BIR_ADD_OV: case BIR_SUB_OV: case BIR_MUL_OV: return IselOtimOverflow(c, i);
    case BIR_CONST: case BIR_COPIA: case BIR_TRUNC: case BIR_EXT_S: case BIR_EXT_Z: return real ? IselOtimReal(c, i) : IselOtimMover(c, i);
    case BIR_NEG: return real ? IselOtimReal(c, i) : IselOtimUnaria(c, i);
    case BIR_NOT: return IselOtimUnaria(c, i);
    case BIR_REM: return DividirInteiro(c, i);
    case BIR_SHL: case BIR_SHR: return IselOtimDeslocaDivide(c, i);
    case BIR_RAIZ: case BIR_I_PARA_F: case BIR_F_PARA_F: return IselOtimReal(c, i);
    case BIR_CARREGA: return IselOtimCarregaSimples(c, i);
    default: return 0;
    }
}

/* "t = op ...; p = COPIA t" com t usado so ali: a conta ja sai no registrador de p (a copia some). Nada entra entre as duas instrucoes, entao escrever p uma instrucao antes
 * nao muda nada que alguem leia; *_OV so quando p nao e operando (o frio le os operandos: a regra esta em IselOtimOverflow). */
static int32_t TentarDestinoFundido(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *i = &bloco->instrs[k];
    const BirInstr *copia = k + 1u < bloco->n ? &bloco->instrs[k + 1u] : NULL;
    BirInstr com_destino_final;
    int32_t r;

    if (copia == NULL || copia->op != BIR_COPIA || copia->a.tipo != BO_VREG || i->dst == BIR_SEM_VREG || copia->a.id != i->dst || copia->dst == i->dst)
        return 0;
    if (!IselOtimTempUnico(c, i->dst) || c->f->classe_vreg[copia->dst] != c->f->classe_vreg[i->dst])
        return 0;
    com_destino_final = *i;
    com_destino_final.dst = copia->dst;
    r = DespacharUma(c, &com_destino_final);
    return r > 0 ? 2 : r;
}

static int32_t DespacharValor(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *i = &bloco->instrs[k];
    int32_t r;

    if ((i->op == BIR_ADD || i->op == BIR_MUL) && i->classe == BC_I64) {
        r = IselOtimMemoria(c, bloco, k); /* pode abrir uma cadeia de endereco que termina em CARREGA/GRAVA */
        if (r != 0)
            return r;
    }
    r = TentarDestinoFundido(c, bloco, k);
    return r != 0 ? r : DespacharUma(c, i);
}

static int32_t DespacharFluxoEMemoria(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *i = &bloco->instrs[k];

    switch (i->op) {
    case BIR_CMP:
        if (k + 1u < bloco->n) {
            int32_t fundido = IselOtimComparaDesvia(c, bloco, k);

            if (fundido != 0)
                return fundido;
        }
        return IselOtimCompara(c, i);
    case BIR_DESVIA: return IselOtimDesvia(c, i);
    case BIR_GRAVA: return IselOtimMemoria(c, bloco, k);
    case BIR_END_SIMBOLO: return IselOtimEndSimbolo(c, i);
    case BIR_END_SLOT: return IselOtimEndSlot(c, i);
    default: return 0;
    }
}

/* S2: a instrucao le ou escreve um vetor V128 (dst, operando a ou b)? */
static bool UsaVetor(const BirInstr *i)
{
    return i->classe == BC_V128 || i->a.classe == BC_V128 || i->b.classe == BC_V128;
}

/* Vetor: so CARREGA e GRAVA (com a cadeia de endereco fundida, como em F32/F64) passam por aqui; COPIA, SPLAT e ADD SUB MUL DIV ficam para a selecao comum, que ja usa o registrador do
 * dst e os operandos vivos em xmm (nunca o caminho de inteiro ou de real escalar). */
static int32_t DespacharVetor(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *i = &bloco->instrs[k];

    if (i->op == BIR_CARREGA)
        return IselOtimCarregaSimples(c, i);
    return i->op == BIR_GRAVA ? IselOtimMemoria(c, bloco, k) : 0;
}

int32_t IselOtimTentar(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *i = &bloco->instrs[k];
    int32_t r;

    c->instr = i;
    if (IselPlantio(c, i) != 0)
        return 0; /* plantio de teste: a selecao comum e que sabe errar de proposito */
    if (UsaVetor(i))
        return DespacharVetor(c, bloco, k);
    r = DespacharValor(c, bloco, k);
    if (r != 0)
        return r;
    return DespacharFluxoEMemoria(c, bloco, k);
}
