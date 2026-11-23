/* x64_isel.c - selecao de instrucoes x86-64 por tabela de dados (REGRAS_ISEL[BIR_QTD]): operacoes de valor e conta. O fluxo (chamada,
 * salto, desvio, retorno) mora em x64_isel_fluxo.c.
 * Versao: 0.7.6
 * Changelog: 0.7.6 - S2 (D0703): V128: IselAritmetica -> IselContaVetorial, IselMover -> IselCopiaVetor e REGRAS_ISEL[BIR_SPLAT] = IselSplat (x64_isel_real.c).
 *            0.7.5 - R5 (SIMD): REGRAS_ISEL[BIR_SELECIONA] = IselSeleciona (x64_isel_otim_fluxo.c).
 *            0.7.4 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.7.3 - M7a F5: CC_DA_COND e FORMAS_ARIT exportadas (x64_isel_otim_fluxo.c); a selecao pos-alocador (passe `isel`) mora em x64_isel_otim*.c.
 *            0.7.2 - M7a F2: carga e guarda de vreg inteiro por AlocOperandoDe/AlocCarregar/AlocGravar.
 *            0.7.1 - M3 F9b-1: decisoes extraidas de IselCmp.
 *            0.7.0 - M3 P21v-c: REGRAS_ISEL[BIR_CHAMA_IND] = IselChama (x64_isel_fluxo.c).
 *            0.6.0 - Linguagem v1.11 L13: REGRAS_ISEL[BIR_RAIZ] = IselRaiz (x64_isel_real.c).
 *            0.5.0 - M2 P18b: IselPlantio (FORJA_TESTE_PLANTAR=1|2|3, so teste: ADD I32 imm 7 vira SUB, CMP LT_S vira LE_S, o retorno de forja_programa soma 1).
 *            0.4.0 - M2 P11a: a tabela ganha END_SLOT, CARREGA, GRAVA, COPIA_MEM e ZERA_MEM (x64_isel_mem.c).
 *            0.3.0 - M2 P10b: IselCarregar/IselGuardar despacham para xmm quando a classe e real; ADD SUB MUL, NEG e CMP de real vao para x64_isel_real.c; tabela ganha
 *                    I_PARA_F e F_PARA_F.
 *            0.2.0 - M2 P9: I16 (CarregarCurto: movzx de 8/16 bits ou movsx a 64 com BE_SINAL; MOV m16,r16 na guarda), IselOperacao publica com a extensao dos
 *                    operandos (comparacao com sinal em I8/I16 alarga por movsx), IselExtensaoDaConta, e a tabela ganha DIV REM SHL SHR NOT
 *                    (x64_isel_conta.c). O ADD_OV/SUB_OV/MUL_OV/NEG_OV de I8 e I16 vai para IselOvCurto.
 *            0.1.0 - backend x64 P7a: CONST COPIA ADD SUB MUL AND OR XOR NEG CMP EXT_S EXT_Z TRUNC, ADD_OV SUB_OV MUL_OV NEG_OV e END_SIMBOLO,
 *                    com a alocacao do degrau 0 (carrega do slot em rax/rcx, opera, guarda no slot).
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 *
 * Desvio do contrato: o CMP termina em "setcc al" e guarda o byte; o movzx do contrato fica de fora porque o slot de classe I8 e lido
 * sempre por movzx de 8 bits (nunca como palavra).
 * Dev: com sinal, ADD/SUB/MUL (e NEG, que a BIR traz como 0 - x) usam a operacao e "jo" para o bloco frio; sem sinal, ADD/SUB usam
 * "jc" e o MUL usa "mul r" + "jo". O resultado so vai ao slot depois do desvio, entao o bloco frio ainda ve os operandos originais.
 */
#include <stdio.h>
#include <stdlib.h>

#include "nativo/x64/x64_interno.h"

/* Formas da operacao de dois operandos (FormasAritmeticas, x64_interno.h); a selecao pos-alocador (x64_isel_otim.c) as le tambem. */
const FormasAritmeticas FORMAS_ARIT[BIR_QTD] = {
    [BIR_ADD] = {{FX_ADD32_RR, FX_ADD64_RR}, {FX_ADD32_RI, FX_ADD64_RI}},
    [BIR_SUB] = {{FX_SUB32_RR, FX_SUB64_RR}, {FX_SUB32_RI, FX_SUB64_RI}},
    [BIR_AND] = {{FX_AND32_RR, FX_AND64_RR}, {FX_AND32_RI, FX_AND64_RI}},
    [BIR_OR] = {{FX_OR32_RR, FX_OR64_RR}, {FX_OR32_RI, FX_OR64_RI}},
    [BIR_XOR] = {{FX_XOR32_RR, FX_XOR64_RR}, {FX_XOR32_RI, FX_XOR64_RI}},
    [BIR_CMP] = {{FX_CMP32_RR, FX_CMP64_RR}, {FX_CMP32_RI, FX_CMP64_RI}},
    [BIR_MUL] = {{FX_IMUL32_RR, FX_IMUL64_RR}, {FX_QTD, FX_QTD}}, /* imul nao tem forma com imediato */
};

/* A operacao com checagem de estouro e a base que ela calcula; NEG_OV chega da BIR como 0 - x. */
static const uint8_t BASE_DO_OV[BIR_QTD] = {
    [BIR_ADD_OV] = BIR_ADD, [BIR_SUB_OV] = BIR_SUB, [BIR_MUL_OV] = BIR_MUL, [BIR_NEG_OV] = BIR_SUB,
};

/* BirCond -> condicao do setcc: iguais, com sinal (l, le, g, ge) e sem sinal (b, be, a, ae). */
const X64Cond CC_DA_COND[10] = {
    X64_CC_E, X64_CC_NE, X64_CC_L, X64_CC_LE, X64_CC_G, X64_CC_GE, X64_CC_B, X64_CC_BE, X64_CC_A, X64_CC_AE,
};

/* ---- carga e guarda de vreg ---- */

/* Imediato como a instrucao o enxerga: BE_SINAL e BE_ZERO alargam a classe a 64 bits; sem extensao vale o padrao da classe. */
static int64_t ImediatoI8(int64_t imed, uint8_t ext)
{
    return ext == BE_SINAL ? (int64_t)(int8_t)imed : (int64_t)(uint8_t)imed;
}

static int64_t ImediatoI16(int64_t imed, uint8_t ext)
{
    return ext == BE_SINAL ? (int64_t)(int16_t)imed : (int64_t)(uint16_t)imed;
}

static int64_t ImediatoI32(int64_t imed, uint8_t ext)
{
    return ext == BE_ZERO ? (int64_t)(uint32_t)imed : (int64_t)(int32_t)imed;
}

static int64_t ImediatoNormalizado(const BirOperando *o, uint8_t ext)
{
    switch (o->classe) {
    case BC_I8: return ImediatoI8(o->imed, ext);
    case BC_I16: return ImediatoI16(o->imed, ext);
    case BC_I32: return ImediatoI32(o->imed, ext);
    default: return o->imed;
    }
}

static bool CarregarImediato(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    int64_t valor = ImediatoNormalizado(o, ext);

    if (o->classe == BC_I64 || ext != BE_NENHUMA)
        return X64Emitir(c, MI2(FX_MOV64_RI, MReg(reg, 64), MImed(valor)));
    return X64Emitir(c, MI2(FX_MOV32_RI, MReg(reg, 32), MImed((int32_t)valor)));
}

/* I8 e I16 leem a largura do slot: movzx (zera ate 32 bits, e a metade alta do registro) ou, com BE_SINAL, movsx a 64 bits. O bool (0/1) e I8. */
static bool CarregarCurto(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    bool byte = o->classe == BC_I8;
    AlocOperando vreg = AlocOperandoDe(c->f, o->id, byte ? 8 : 16);

    if (ext == BE_SINAL)
        return AlocCarregar(c, byte ? FX_MOVSX64_RM8 : FX_MOVSX64_RM16, MReg(reg, 64), &vreg);
    return AlocCarregar(c, byte ? FX_MOVZX32_RM8 : FX_MOVZX32_RM16, MReg(reg, 32), &vreg);
}

/* I32 carrega em 32 bits (o resto do registrador zera); BE_SINAL acrescenta movsxd. */
static bool CarregarVreg(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    AlocOperando vreg64 = AlocOperandoDe(c->f, o->id, 64);
    AlocOperando vreg32 = AlocOperandoDe(c->f, o->id, 32);

    switch (o->classe) {
    case BC_I64:
        return AlocCarregar(c, FX_MOV64_RM, MReg(reg, 64), &vreg64);
    case BC_I32:
        if (!AlocCarregar(c, FX_MOV32_RM, MReg(reg, 32), &vreg32))
            return false;
        return ext != BE_SINAL || X64Emitir(c, MI2(FX_MOVSXD64_RR32, MReg(reg, 64), MReg(reg, 32)));
    case BC_I8:
    case BC_I16:
        return CarregarCurto(c, reg, o, ext);
    default:
        return X64SemRegra(c, "vreg", "operando de classe fora do M2 (so i8, i16, i32 e i64)");
    }
}

bool IselCarregar(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    if (BirClasseEhReal(o->classe) && o->tipo != BO_NADA)
        return IselCarregarReal(c, (uint8_t)reg, o);
    if (o->tipo == BO_IMED)
        return CarregarImediato(c, reg, o, ext);
    if (o->tipo == BO_VREG)
        return CarregarVreg(c, reg, o, ext);
    return X64SemRegra(c, "operando", "esperava vreg ou imediato");
}

bool IselGuardar(X64Ctx *c, uint32_t vreg, X64Reg reg)
{
    uint8_t bits = AlocBits(c->f->classe_vreg[vreg]);
    uint16_t forma = bits == 64 ? FX_MOV64_MR : (bits == 32 ? FX_MOV32_MR : (bits == 16 ? FX_MOV16_MR : FX_MOV8_MR));

    if (BirClasseEhReal(c->f->classe_vreg[vreg]))
        return IselGuardarReal(c, vreg, (uint8_t)reg);
    AlocOperando destino = AlocOperandoDe(c->f, vreg, bits);

    return AlocGravar(c, forma, &destino, MReg(reg, bits));
}

/* Extensao que a conta de 32 bits pede aos operandos curtos: com sinal, I8 e I16 alargam por movsx; os demais casos nao precisam (I32 opera em 32). */
uint8_t IselExtensaoDaConta(uint8_t classe, bool com_sinal)
{
    return com_sinal && (classe == BC_I8 || classe == BC_I16) ? BE_SINAL : BE_NENHUMA;
}

/* ---- dois operandos: rax = rax op b ---- */

/* Imediato que cabe no campo imm32 (sinal estendido a 64 nas operacoes de 64 bits). */
static bool ImediatoPequeno(const BirOperando *o, uint8_t ext, int64_t *valor)
{
    if (o->tipo != BO_IMED)
        return false;
    *valor = ImediatoNormalizado(o, ext);
    return *valor >= INT32_MIN && *valor <= INT32_MAX;
}

/* 1 para 64 bits, 0 para 32 (I8, I16 e I32 operam em 32). */
uint8_t IselIndiceLargura(uint8_t classe)
{
    return classe == BC_I64;
}

/* rax = rax <base> b, com os operandos alargados por ext (so I8 e I16 usam; veja IselExtensaoDaConta). */
bool IselOperacao(X64Ctx *c, const BirInstr *i, uint8_t base, uint8_t ext)
{
    const FormasAritmeticas *formas = &FORMAS_ARIT[base];
    uint8_t larg = IselIndiceLargura(i->a.classe);
    uint8_t bits = (uint8_t)(32u << larg);
    int64_t imed;

    if (!IselCarregar(c, X64_RAX, &i->a, ext))
        return false;
    if (formas->ri[larg] != FX_QTD && ImediatoPequeno(&i->b, ext, &imed))
        return X64Emitir(c, MI2(formas->ri[larg], MReg(X64_RAX, bits), MImed(imed)));
    if (!IselCarregar(c, X64_RCX, &i->b, ext))
        return false;
    return X64Emitir(c, MI2(formas->rr[larg], MReg(X64_RAX, bits), MReg(X64_RCX, bits)));
}

/* ---- plantio de teste (P18b): divergencias de proposito para provar o diferencial; sem a variavel nada muda ---- */

static bool SomaDeI32ComSete(const BirInstr *i)
{
    bool soma = i->op == BIR_ADD || i->op == BIR_ADD_OV;

    return soma && i->a.classe == BC_I32 && i->b.tipo == BO_IMED && i->b.imed == 7;
}

/* O plantio pedido: 1..3 (um digito so); 0 = variavel ausente ou fora disso. */
static int PlantioPedido(void)
{
    const char *env = getenv("FORJA_TESTE_PLANTAR");

    if (env == NULL || env[0] == '\0' || env[1] != '\0')
        return 0;
    return env[0] >= '1' && env[0] <= '3' ? env[0] - '0' : 0;
}

static bool PlantioValeAqui(int qual, const X64Ctx *c, const BirInstr *i)
{
    switch (qual) {
    case 1: return SomaDeI32ComSete(i);
    case 2: return i->op == BIR_CMP && i->aux == BCOND_LT_S;
    case 3: return i->op == BIR_RETORNA && c->f->sintetica;
    default: return false;
    }
}

uint8_t IselPlantio(const X64Ctx *c, const BirInstr *i)
{
    static int qual = -1; /* lida uma vez por processo: a selecao chama isto por instrucao */

    if (qual < 0) {
        qual = PlantioPedido();
        if (qual > 0) /* o plantio muda o codigo de proposito: nunca em silencio */
            fprintf(stderr, "aviso: FORJA_TESTE_PLANTAR=%d ativo: o codigo gerado esta errado de proposito (so para testar o diferencial)\n", qual);
    }
    return PlantioValeAqui(qual, c, i) ? (uint8_t)qual : 0;
}

static bool IselAritmetica(X64Ctx *c, const BirInstr *i)
{
    uint8_t base = IselPlantio(c, i) == 1 ? BIR_SUB : i->op;

    if (i->classe == BC_V128)
        return IselContaVetorial(c, i);
    if (BirClasseEhReal(i->classe))
        return IselContaReal(c, i);
    return IselOperacao(c, i, base, BE_NENHUMA) && IselGuardar(c, i->dst, X64_RAX);
}

/* MUL sem sinal: rdx:rax = rax * rcx; CF e OF acendem se a metade alta nao e zero. */
static bool MultiplicaSemSinal(X64Ctx *c, const BirInstr *i)
{
    uint8_t larg = IselIndiceLargura(i->a.classe);

    if (!IselCarregar(c, X64_RAX, &i->a, BE_NENHUMA) || !IselCarregar(c, X64_RCX, &i->b, BE_NENHUMA))
        return false;
    return X64Emitir(c, MI1(larg ? FX_MUL64_R : FX_MUL32_R, MReg(X64_RCX, (uint8_t)(32u << larg))));
}

/* Com sinal (e no MUL sem sinal, que usa "mul") o estouro acende OF; ADD e SUB sem sinal acendem CF. */
static X64Cond CondicaoDoOv(const BirInstr *i, uint8_t base)
{
    return (i->aux == BIR_SINAL || base == BIR_MUL) ? X64_CC_O : X64_CC_B;
}

static bool CorpoDoOv(X64Ctx *c, const BirInstr *i, uint8_t base)
{
    if (base == BIR_MUL && i->aux == BIR_SEM_SINAL)
        return MultiplicaSemSinal(c, i);
    return IselOperacao(c, i, base, BE_NENHUMA);
}

/* I8 e I16 passam por x64_isel_conta.c (conta em 32 bits e faixa do tipo); I32 e I64 usam o flag de overflow do hardware. */
static bool IselOv(X64Ctx *c, const BirInstr *i)
{
    uint8_t base = IselPlantio(c, i) == 1 ? BIR_SUB : BASE_DO_OV[i->op];

    if (i->classe == BC_I8 || i->classe == BC_I16)
        return IselOvCurto(c, i, base);
    if (!CorpoDoOv(c, i, base))
        return false;
    if (!X64Emitir(c, MICc(FX_JCC_REL32, CondicaoDoOv(i, base), MBloco(i->alvo[0]))))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

static bool CondicaoComSinal(uint8_t cond)
{
    return cond >= BCOND_LT_S && cond <= BCOND_GE_S;
}

static bool IselCmp(X64Ctx *c, const BirInstr *i)
{
    uint8_t cond = IselPlantio(c, i) == 2 ? BCOND_LE_S : i->aux;

    if (BirClasseEhReal(i->a.classe))
        return IselCmpReal(c, i);
    if (i->aux >= sizeof CC_DA_COND / sizeof CC_DA_COND[0])
        return X64SemRegra(c, "cmp", "condicao fora das 10 do BirCond");
    if (!IselOperacao(c, i, BIR_CMP, IselExtensaoDaConta(i->a.classe, CondicaoComSinal(i->aux))))
        return false;
    if (!X64Emitir(c, MICc(FX_SETCC8_R, CC_DA_COND[cond], MReg(X64_RAX, 8))))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

/* ---- um operando ---- */

static bool IselNeg(X64Ctx *c, const BirInstr *i)
{
    uint8_t larg = IselIndiceLargura(i->classe);

    if (BirClasseEhReal(i->classe))
        return IselNegReal(c, i);
    if (!IselCarregar(c, X64_RAX, &i->a, BE_NENHUMA))
        return false;
    if (!X64Emitir(c, MI1(larg ? FX_NEG64_R : FX_NEG32_R, MReg(X64_RAX, (uint8_t)(32u << larg)))))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

/* CONST, COPIA e TRUNC: carrega a (na largura da classe dele) e guarda os bits baixos na classe do dst. */
static bool IselMover(X64Ctx *c, const BirInstr *i)
{
    if (i->classe == BC_V128) /* S2: COPIA de vetor */
        return IselCopiaVetor(c, i);
    return IselCarregar(c, X64_RAX, &i->a, BE_NENHUMA) && IselGuardar(c, i->dst, X64_RAX);
}

static bool IselExtensaoSinal(X64Ctx *c, const BirInstr *i)
{
    return IselCarregar(c, X64_RAX, &i->a, BE_SINAL) && IselGuardar(c, i->dst, X64_RAX);
}

static bool IselExtensaoZero(X64Ctx *c, const BirInstr *i)
{
    return IselCarregar(c, X64_RAX, &i->a, BE_ZERO) && IselGuardar(c, i->dst, X64_RAX);
}

static bool IselEndSimbolo(X64Ctx *c, const BirInstr *i)
{
    MOperando alvo = MMem(X64_RIP, 0, 64);

    alvo.simbolo = c->obj_sim[i->a.id];
    if (!X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RAX, 64), alvo)))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

/* ---- a tabela ---- */

const RegraIsel REGRAS_ISEL[BIR_QTD] = {
    [BIR_CONST] = {"const", IselMover}, [BIR_COPIA] = {"copia", IselMover}, [BIR_TRUNC] = {"trunc", IselMover},
    [BIR_ADD] = {"add", IselAritmetica}, [BIR_SUB] = {"sub", IselAritmetica}, [BIR_MUL] = {"mul", IselAritmetica},
    [BIR_AND] = {"and", IselAritmetica}, [BIR_OR] = {"or", IselAritmetica}, [BIR_XOR] = {"xor", IselAritmetica},
    [BIR_NEG] = {"neg", IselNeg}, [BIR_CMP] = {"cmp", IselCmp},
    [BIR_EXT_S] = {"ext_s", IselExtensaoSinal}, [BIR_EXT_Z] = {"ext_z", IselExtensaoZero},
    [BIR_ADD_OV] = {"add_ov", IselOv}, [BIR_SUB_OV] = {"sub_ov", IselOv}, [BIR_MUL_OV] = {"mul_ov", IselOv},
    [BIR_NEG_OV] = {"neg_ov", IselOv}, [BIR_END_SIMBOLO] = {"end", IselEndSimbolo},
    [BIR_CHAMA] = {"chama", IselChama}, [BIR_SALTA] = {"salta", IselSalta}, [BIR_DESVIA] = {"desvia", IselDesvia},
    [BIR_RETORNA] = {"retorna", IselRetorna}, [BIR_ARMADILHA] = {"armadilha", IselArmadilha},
    [BIR_DIV] = {"div", IselDivisao}, [BIR_REM] = {"rem", IselDivisao}, [BIR_SHL] = {"shl", IselDeslocamento},
    [BIR_SHR] = {"shr", IselDeslocamento}, [BIR_NOT] = {"not", IselNot},
    [BIR_I_PARA_F] = {"i_para_f", IselIParaF}, [BIR_F_PARA_F] = {"f_para_f", IselFParaF}, [BIR_RAIZ] = {"raiz", IselRaiz},
    [BIR_CHAMA_IND] = {"chama_ind", IselChama}, [BIR_SELECIONA] = {"seleciona", IselSeleciona}, [BIR_SPLAT] = {"splat", IselSplat},
    [BIR_END_SLOT] = {"end_slot", IselEndSlot}, [BIR_CARREGA] = {"carrega", IselCarrega}, [BIR_GRAVA] = {"grava", IselGrava},
    [BIR_COPIA_MEM] = {"copia_mem", IselCopiaMem}, [BIR_ZERA_MEM] = {"zera_mem", IselZeraMem},
};

bool X64Selecionar(X64Ctx *c, const BirInstr *i)
{
    char numero[16];

    c->instr = i;
    if (i->op < BIR_QTD && REGRAS_ISEL[i->op].fn != NULL)
        return REGRAS_ISEL[i->op].fn(c, i);
    snprintf(numero, sizeof numero, "#%u", (unsigned)i->op);
    return X64SemRegra(c, numero, "operacao fora da tabela REGRAS_ISEL");
}
