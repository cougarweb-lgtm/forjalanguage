/* x64_isel_otim_mem.c - selecao pos-alocador (M7a F5): CARREGA/GRAVA com a base e o valor direto nos registradores vivos, modo [base+indice*escala+desloc] montado a partir
 * de ADD/MUL adjacentes e END_SIMBOLO/END_SLOT calculados no registrador do dst.
 * Versao: 0.1.2
 * Changelog: 0.1.2 - S2 (D0703): CARREGA e GRAVA de V128 por movups (EmitirCarga/EmitirGravacao), tambem com a cadeia de endereco [base+indice*escala+desloc] fundida.
 *            0.1.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.1.0 - M7a F5: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F4> --release-muda
 *
 * Cadeias de endereco (todas de I64, cada temporario com uma definicao e um uso, as instrucoes ADJACENTES e os operandos de origem em registrador):
 *   A: MUL t1 = i*{1,2,4,8}; ADD t2 = x + t1; MEM [t2+d]            -> [x + i*s + d]
 *   B: ADD t0 = x + imm; MUL t1 = i*s; ADD t2 = t0 + t1; MEM [t2+d]  -> [x + i*s + imm + d]
 *   C: ADD t2 = x + y; MEM [t2+d]                                    -> [x + y + d]
 *   D: ADD t0 = x + imm; MEM [t0+d]                                  -> [x + imm + d]
 * Nada se emite no meio da cadeia, entao x e i so sao lidos na instrucao final: o intervalo deles pode ter acabado na primeira instrucao da cadeia sem risco (a unica
 * escrita possivel num registrador de x ou i e o dst do CARREGA, depois da leitura, na mesma instrucao da maquina).
 */
#include "nativo/x64/x64_isel_otim.h"

#define SEM_INDICE (-1)

typedef struct { X64Reg base; int indice; uint8_t escala; int64_t desloc; uint32_t instrucoes; } Cadeia;

/* ---- casamento da cadeia ---- */

static const BirInstr *Adiante(const BirBloco *bloco, uint32_t k, uint32_t d)
{
    return k + d < bloco->n ? &bloco->instrs[k + d] : NULL;
}

static bool TempI64(const X64Ctx *c, uint32_t vreg)
{
    return vreg < c->f->nvregs && c->f->classe_vreg[vreg] == BC_I64 && IselOtimTempUnico(c, vreg);
}

static bool EhOp64(const BirInstr *i, uint8_t op)
{
    return i != NULL && i->op == op && i->classe == BC_I64 && i->a.classe == BC_I64 && i->b.classe == BC_I64;
}

static bool UsaTemp(const BirOperando *o, uint32_t t)
{
    return o->tipo == BO_VREG && o->id == t;
}

/* MUL t = reg * {1,2,4,8} (o imediato em qualquer um dos lados), t temporario unico: devolve o registrador do indice e a escala. */
static bool CasarMulEscala(const X64Ctx *c, const BirInstr *i, int *indice, uint8_t *escala)
{
    const BirOperando *imediato = i->b.tipo == BO_IMED ? &i->b : &i->a;
    const BirOperando *outro = imediato == &i->b ? &i->a : &i->b;
    int64_t s = imediato->imed;

    if (!EhOp64(i, BIR_MUL) || !TempI64(c, i->dst) || imediato->tipo != BO_IMED || outro->tipo != BO_VREG)
        return false;
    if (s != 1 && s != 2 && s != 4 && s != 8)
        return false;
    *indice = IselOtimRegInt(c, outro);
    *escala = (uint8_t)s;
    return *indice >= 0;
}

/* ADD t2 = x + t (t temporario ja casado, em qualquer lado): devolve o outro operando. */
static const BirOperando *OutroDaSoma(const BirInstr *soma, uint32_t t)
{
    if (UsaTemp(&soma->a, t) && !UsaTemp(&soma->b, t))
        return &soma->b;
    return UsaTemp(&soma->b, t) && !UsaTemp(&soma->a, t) ? &soma->a : NULL;
}

/* A instrucao final (CARREGA/GRAVA) le o endereco t como base? */
static bool BaseEhTemp(const BirInstr *mem, uint32_t t)
{
    return mem != NULL && (mem->op == BIR_CARREGA || mem->op == BIR_GRAVA) && UsaTemp(&mem->a, t) && !(mem->op == BIR_GRAVA && UsaTemp(&mem->b, t));
}

/* ADD t = x + imm32 (imediato em qualquer lado), x em registrador, t temporario unico: base e deslocamento. */
static bool CasarSomaImediata(const X64Ctx *c, const BirInstr *i, int *base, int64_t *desloc)
{
    const BirOperando *imediato = i->b.tipo == BO_IMED ? &i->b : &i->a;
    const BirOperando *outro = imediato == &i->b ? &i->a : &i->b;

    if (!EhOp64(i, BIR_ADD) || !TempI64(c, i->dst) || imediato->tipo != BO_IMED || outro->tipo != BO_VREG)
        return false;
    if (imediato->imed < INT32_MIN || imediato->imed > INT32_MAX)
        return false;
    *base = IselOtimRegInt(c, outro);
    *desloc = imediato->imed;
    return *base >= 0;
}

/* Forma A: MUL, ADD, MEM a partir de k. */
static bool CasarFormaA(const X64Ctx *c, const BirBloco *bloco, uint32_t k, Cadeia *r)
{
    const BirInstr *mul = Adiante(bloco, k, 0), *soma = Adiante(bloco, k + 1u, 0), *mem = Adiante(bloco, k + 2u, 0);
    const BirOperando *x;
    int base;

    if (soma == NULL || mem == NULL || !CasarMulEscala(c, mul, &r->indice, &r->escala) || !EhOp64(soma, BIR_ADD) || !TempI64(c, soma->dst))
        return false;
    x = OutroDaSoma(soma, mul->dst);
    base = x != NULL && x->tipo == BO_VREG ? IselOtimRegInt(c, x) : -1;
    if (base < 0 || !BaseEhTemp(mem, soma->dst))
        return false;
    r->base = (X64Reg)base;
    r->desloc = 0;
    r->instrucoes = 3;
    return true;
}

/* Forma B: ADD imm, MUL, ADD, MEM a partir de k. */
static bool CasarFormaB(const X64Ctx *c, const BirBloco *bloco, uint32_t k, Cadeia *r)
{
    const BirInstr *s0 = Adiante(bloco, k, 0), *mul = Adiante(bloco, k + 1u, 0), *soma = Adiante(bloco, k + 2u, 0), *mem = Adiante(bloco, k + 3u, 0);
    const BirOperando *outro;
    int base;

    if (mul == NULL || soma == NULL || mem == NULL || !CasarSomaImediata(c, s0, &base, &r->desloc) || !CasarMulEscala(c, mul, &r->indice, &r->escala))
        return false;
    if (!EhOp64(soma, BIR_ADD) || !TempI64(c, soma->dst) || !BaseEhTemp(mem, soma->dst))
        return false;
    outro = OutroDaSoma(soma, s0->dst);
    if (outro == NULL || !UsaTemp(outro, mul->dst))
        return false;
    r->base = (X64Reg)base;
    r->instrucoes = 4;
    return true;
}

/* Formas C e D: ADD t = x + y (indice, escala 1) ou ADD t = x + imm, seguida do MEM. */
static bool CasarFormaCD(const X64Ctx *c, const BirBloco *bloco, uint32_t k, Cadeia *r)
{
    const BirInstr *soma = Adiante(bloco, k, 0), *mem = Adiante(bloco, k + 1u, 0);
    int base, y;

    if (mem == NULL || !EhOp64(soma, BIR_ADD) || !TempI64(c, soma->dst) || !BaseEhTemp(mem, soma->dst))
        return false;
    r->instrucoes = 2;
    r->indice = SEM_INDICE;
    r->escala = 0;
    if (CasarSomaImediata(c, soma, &base, &r->desloc)) {
        r->base = (X64Reg)base;
        return true;
    }
    base = soma->a.tipo == BO_VREG ? IselOtimRegInt(c, &soma->a) : -1;
    y = soma->b.tipo == BO_VREG ? IselOtimRegInt(c, &soma->b) : -1;
    if (base < 0 || y < 0)
        return false;
    r->base = (X64Reg)base;
    r->indice = y;
    r->escala = 1;
    r->desloc = 0;
    return true;
}

static bool CasarCadeia(const X64Ctx *c, const BirBloco *bloco, uint32_t k, Cadeia *r)
{
    const BirInstr *i = &bloco->instrs[k];

    if (i->op == BIR_MUL)
        return CasarFormaA(c, bloco, k, r);
    if (i->op != BIR_ADD)
        return false;
    return CasarFormaB(c, bloco, k, r) || CasarFormaCD(c, bloco, k, r);
}

/* ---- emissao de CARREGA e GRAVA ---- */

/* Largura do acesso em bits: a do dst no CARREGA, a do valor na GRAVA. */
static uint8_t BitsDoAcesso(const BirInstr *mem)
{
    return mem->op == BIR_CARREGA ? AlocBits(mem->classe) : AlocBits(mem->b.classe);
}

/* Forma de memoria: da cadeia (+ o desloc da instrucao) ou a base do operando a (em registrador vivo ou carregada em rcx). */
static bool MontarMemoria(X64Ctx *c, const BirInstr *mem, const Cadeia *cadeia, MOperando *saida)
{
    uint8_t bits = BitsDoAcesso(mem);
    X64Reg base;
    bool ok;

    if (cadeia != NULL) {
        int64_t total = cadeia->desloc + mem->desloc;

        if (total < INT32_MIN || total > INT32_MAX)
            return false;
        *saida = MMem(cadeia->base, (int32_t)total, bits);
        if (cadeia->indice != SEM_INDICE) {
            saida->indice = (X64Reg)cadeia->indice;
            saida->escala = cadeia->escala;
        }
        return IselOtimEncodavel(saida);
    }
    base = IselOtimFonte(c, &mem->a, X64_RCX, &ok);
    *saida = MMem(base, mem->desloc, bits);
    return ok;
}

/* [bits][carga]: forma de carga de inteiro por largura do dado (8, 16, 32 e 64) e a largura do registrador que ela escreve. */
static uint16_t FormaDeCarga(uint8_t bits, uint8_t *largura_reg)
{
    *largura_reg = bits == 64 ? 64 : 32;
    switch (bits) {
    case 64: return FX_MOV64_RM;
    case 32: return FX_MOV32_RM;
    case 16: return FX_MOVZX32_RM16;
    default: return FX_MOVZX32_RM8;
    }
}

static uint16_t FormaDeGravacao(uint8_t bits)
{
    switch (bits) {
    case 64: return FX_MOV64_MR;
    case 32: return FX_MOV32_MR;
    case 16: return FX_MOV16_MR;
    default: return FX_MOV8_MR;
    }
}

static bool EmitirCarga(X64Ctx *c, const BirInstr *i, MOperando origem)
{
    uint8_t bits = AlocBits(i->classe), largura;
    int xmm = IselOtimRegXmmDestino(c, i->dst);
    int reg = IselOtimRegDestino(c, i->dst);
    X64Reg alvo = reg >= 0 ? (X64Reg)reg : X64_RAX;
    uint16_t forma;

    if (BirClasseEhReal(i->classe)) {
        uint8_t destino = xmm >= 0 ? (uint8_t)xmm : 0;

        if (!X64Emitir(c, MI2(i->classe == BC_F64 ? FX_MOVSD_XM : FX_MOVSS_XM, MXmm(destino), origem)))
            return false;
        return xmm >= 0 || IselGuardarReal(c, i->dst, 0);
    }
    forma = FormaDeCarga(bits, &largura);
    if (!X64Emitir(c, MI2(forma, MReg(alvo, largura), origem)))
        return false;
    return reg >= 0 || IselGuardar(c, i->dst, X64_RAX);
}

static bool EmitirGravacao(X64Ctx *c, const BirInstr *i, MOperando destino)
{
    uint8_t bits = AlocBits(i->b.classe);
    X64Reg fonte;
    bool ok;
    int xmm;

    if (BirClasseEhReal(i->b.classe)) {
        xmm = IselOtimRegXmm(c, &i->b);
        if (xmm < 0 && !IselCarregarReal(c, 0, &i->b))
            return false;
        return IselGuardarXmm(c, i->b.classe, destino, xmm >= 0 ? (uint8_t)xmm : 0);
    }
    fonte = IselOtimFonte(c, &i->b, X64_RAX, &ok);
    return ok && X64Emitir(c, MI2(FormaDeGravacao(bits), destino, MReg(fonte, bits)));
}

/* S2: CARREGA de V128 por movups direto no xmm do dst, ou no rascunho xmm0 e ao home de 16 B. */
static bool EmitirCargaVetor(X64Ctx *c, const BirInstr *i, MOperando origem)
{
    int xmm = IselOtimRegXmmDestino(c, i->dst);

    if (!X64Emitir(c, MI2(FX_MOVUPS_XM, MXmm(xmm >= 0 ? (uint8_t)xmm : 0), origem)))
        return false;
    return xmm >= 0 || IselGuardarVetor(c, i->dst, 0);
}

/* S2: GRAVA de V128 por movups do xmm vivo ou do home (carregado em xmm0). */
static bool EmitirGravacaoVetor(X64Ctx *c, const BirInstr *i, MOperando destino)
{
    int xmm = IselOtimRegXmm(c, &i->b);

    if (xmm < 0 && !IselCarregarVetor(c, 0, i->b.id))
        return false;
    return X64Emitir(c, MI2(FX_MOVUPS_MX, destino, MXmm(xmm >= 0 ? (uint8_t)xmm : 0)));
}

/* Emite CARREGA/GRAVA; a cadeia (ou NULL) diz o endereco; devolve n instrucoes consumidas. */
static int32_t EmitirMemoria(X64Ctx *c, const BirInstr *mem, const Cadeia *cadeia, uint32_t n)
{
    MOperando operando;

    if (BitsDoAcesso(mem) == 0 || !MontarMemoria(c, mem, cadeia, &operando))
        return cadeia == NULL ? -1 : 0;
    if (cadeia != NULL)
        c->instr = mem; /* sem cadeia, c->instr ja e a instrucao do bloco: mem pode ser uma copia na pilha (TentarDestinoFundido) e a listagem subtrai ponteiros */
    if (mem->op == BIR_CARREGA)
        return (mem->classe == BC_V128 ? EmitirCargaVetor(c, mem, operando) : EmitirCarga(c, mem, operando)) ? (int32_t)n : -1;
    return (mem->b.classe == BC_V128 ? EmitirGravacaoVetor(c, mem, operando) : EmitirGravacao(c, mem, operando)) ? (int32_t)n : -1;
}

int32_t IselOtimCarregaSimples(X64Ctx *c, const BirInstr *i)
{
    return EmitirMemoria(c, i, NULL, 1);
}

/* k e uma GRAVA (uma instrucao) ou o inicio de uma cadeia de endereco (ADD/MUL) que termina em CARREGA/GRAVA. */
int32_t IselOtimMemoria(X64Ctx *c, const BirBloco *bloco, uint32_t k)
{
    const BirInstr *i = &bloco->instrs[k];
    Cadeia cadeia = {0};

    if (i->op == BIR_CARREGA || i->op == BIR_GRAVA)
        return EmitirMemoria(c, i, NULL, 1);
    if (!CasarCadeia(c, bloco, k, &cadeia))
        return 0;
    return EmitirMemoria(c, &bloco->instrs[k + cadeia.instrucoes - 1u], &cadeia, cadeia.instrucoes);
}

/* ---- enderecos ---- */

int32_t IselOtimEndSimbolo(X64Ctx *c, const BirInstr *i)
{
    int destino = IselOtimRegDestino(c, i->dst);
    MOperando alvo = MMem(X64_RIP, 0, 64);

    if (destino < 0 || c->f->classe_vreg[i->dst] != BC_I64)
        return 0;
    alvo.simbolo = c->obj_sim[i->a.id];
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg((X64Reg)destino, 64), alvo)) ? 1 : -1;
}

int32_t IselOtimEndSlot(X64Ctx *c, const BirInstr *i)
{
    int destino = IselOtimRegDestino(c, i->dst);

    if (destino < 0 || c->f->classe_vreg[i->dst] != BC_I64)
        return 0;
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg((X64Reg)destino, 64), MMem(X64_RBP, -(int32_t)c->slot_off[i->a.imed], 64))) ? 1 : -1;
}
