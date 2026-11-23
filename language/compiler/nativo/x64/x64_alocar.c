/* x64_alocar.c - alocacao de vregs, degrau 0: todo vreg vive num slot de 8 bytes da pilha, sem otimizador.
 * Versao: 0.9.0
 * Changelog: 0.9.0 - S2 (D0703): AlocBits(BC_V128) = 128 (AlocClassesSuportadas aceita a classe); AlocCarregar/AlocGravar de FX_MOVUPS_XM/MX em registrador viram movaps xmm,xmm.
 *            0.8.0 - S1b (D0703): home de 16 B do vreg BC_V128 numa regiao propria depois dos slots de vreg (AlocBytesSlots a inclui; sem V128 o quadro e identico). AlocBits(BC_V128) segue 0:
 *                    o isel de vetor e o S2, AlocClassesSuportadas ainda recusa a classe.
 *            0.7.0 - M7a F4: vreg real em xmm (reg guarda o numero do xmm): AlocCarregar/AlocGravar de movss/movsd viram movaps xmm,xmm; AlocRegsSalvosXmm.
 *            0.6.0 - M7a F3: vreg em registrador (BirFuncao.reg_vreg): AlocCarregar/AlocGravar viram mov/movzx/movsx entre registradores, AlocHomeDoParametro, AlocRegsSalvos = BirFuncao.regs_salvos;
 *                    os slots de vreg descem os bytes que o prologo empurra (AlocBytesEmpurrados). Sem reg_vreg (dev, --sem-otim alocador) saem os mesmos bytes.
 *            0.5.0 - M7a F2: AlocSlot vira AlocOperando (reg|mem; hoje sempre mem) com AlocCarregar, AlocGravar, AlocMemoria e AlocRegsSalvos; mesmos bytes.
 *            0.4.0 - M2 P11b: o ponteiro oculto do retorno agregado desloca a posicao dos parametros (AbiWinParamRbp(k + oculto)); parametro agregado de 1/2/4/8 bytes tem o vreg (ponteiro) num
 *                    slot proprio logo depois dos slots de vreg, e AlocBytesSlots os conta.
 *            0.3.0 - M2 P10b: AlocBits(BC_F32) = 32 e AlocBits(BC_F64) = 64 (o slot continua de 8 bytes; movss/movsd leem e gravam a largura).
 *            0.2.0 - M2 P9: AlocBits(BC_I16) = 16 (o slot continua de 8 bytes; a classe de 16 bits le e grava 16).
 *            0.1.0 - backend x64 P7a: slots [rbp-8(j+1)] (j = k - nparams); parametros na home area / pilha do chamador [rbp+16+8k].
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 *
 * Desvio do contrato: o slot do vreg k>=nparams e [rbp-8(k-nparams+1)] (e nao [rbp-8(k+1)]): os parametros ja moram acima do rbp,
 * entao nao ha por que reservar slot abaixo para eles.
 */
#include "nativo/x64/x64_interno.h"

uint8_t AlocBits(uint8_t classe)
{
    switch (classe) {
    case BC_I8: return 8;
    case BC_I16: return 16;
    case BC_I32: return 32;
    case BC_I64: return 64;
    case BC_F32: return 32;
    case BC_F64: return 64;
    case BC_V128: return 128;
    default: return 0;
    }
}

/* Bytes que o prologo empurra logo abaixo do rbp (um push por callee-saved do alocador); os slots de vreg comecam depois deles. */
uint32_t AlocBytesEmpurrados(const BirFuncao *f)
{
    uint32_t n = 0;

    for (uint32_t mascara = f->regs_salvos; mascara != 0; mascara >>= 1)
        n += mascara & 1u;
    return X64_SLOT_BYTES * n;
}

/* Slots de 8 bytes dos vregs (e dos parametros agregados por bytes) mais os pushes do prologo: onde termina a regiao de slots escalares. */
static uint32_t AlocBytesSlotsEscalares(const BirFuncao *f)
{
    return X64_SLOT_BYTES * (f->nvregs - f->nparams + AbiWinOrdemEmBytes(f, f->nparams)) + AlocBytesEmpurrados(f);
}

/* Vregs BC_V128 com indice menor que `ate`: a posicao do home de 16 bytes do vetor na regiao propria (movups, sem exigir alinhamento). */
static uint32_t AlocVetoresAte(const BirFuncao *f, uint32_t ate)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < ate; k++)
        n += f->classe_vreg[k] == BC_V128;
    return n;
}

static int32_t AlocDeslocamento(const BirFuncao *f, uint32_t vreg)
{
    int32_t empurrados = (int32_t)AlocBytesEmpurrados(f);

    if (vreg < f->nvregs && f->classe_vreg[vreg] == BC_V128) /* (vreg fora da faixa: casa sem vreg do --debug) regiao propria depois dos slots de vreg: [rbp - (slots + 16 (j + 1))] */
        return -(int32_t)(AlocBytesSlotsEscalares(f) + X64_VETOR_BYTES * (AlocVetoresAte(f, vreg) + 1u));
    if (vreg < f->nparams && AbiWinParamEmBytes(f, vreg)) /* o vreg guarda o ponteiro para os bytes da home area: slot proprio depois dos de vreg */
        return -(int32_t)(X64_SLOT_BYTES * (f->nvregs - f->nparams + AbiWinOrdemEmBytes(f, vreg) + 1u)) - empurrados;
    if (vreg < f->nparams)
        return AbiWinParamRbp(vreg + AbiWinOculto(f));
    return -(int32_t)(X64_SLOT_BYTES * (vreg - f->nparams + 1u)) - empurrados;
}

AlocOperando AlocOperandoDe(const BirFuncao *f, uint32_t vreg, uint8_t largura)
{
    AlocOperando o = {.em_reg = false, .reg = X64_RAX, .desloc = AlocDeslocamento(f, vreg), .largura = largura};

    if (f->reg_vreg != NULL && f->reg_vreg[vreg] != BIR_SEM_REG) {
        o.em_reg = true;
        o.reg = (X64Reg)f->reg_vreg[vreg];
    }
    return o;
}

/* Forma de memoria do operando (so existe com em_reg == false); o xmm e o quadro leem o slot por aqui. */
MOperando AlocMemoria(const AlocOperando *o)
{
    return MMem(X64_RBP, o->desloc, o->largura);
}

/* Onde o chamador deixou o parametro k (home area ou pilha); o prologo le dali o parametro que o alocador pos em registrador. */
MOperando AlocHomeDoParametro(const BirFuncao *f, uint32_t k, uint8_t largura)
{
    return MMem(X64_RBP, AbiWinParamRbp(k + AbiWinOculto(f)), largura);
}

/* Leitura RM de um vreg em registrador: o registrador guarda o valor estendido com zero ate 64 bits (invariante do alocador), entao movzx vira mov de 32 e o movsx a 64
 * bits passa por movsx de 32 + movsxd. */
static bool AlocCarregarDeRegistrador(X64Ctx *c, uint16_t forma, MOperando destino, const AlocOperando *o)
{
    MOperando fonte = MReg(o->reg, 64);
    MOperando destino32 = MReg(destino.base, 32);

    switch (forma) {
    case FX_MOVSS_XM:
    case FX_MOVSD_XM: /* real: o numero guardado e o do xmm; copia inteira (movaps) nao carrega a dependencia do alto de movss/movsd */
        return X64Emitir(c, MI2(FX_MOVAPS_XX, destino, MXmm((uint8_t)o->reg)));
    case FX_MOVUPS_XM: /* vetor V128 (S2): o home de 16 B vira a copia xmm -> xmm */
        return X64Emitir(c, MI2(FX_MOVAPS_XX, destino, MXmm((uint8_t)o->reg)));
    case FX_MOV64_RM:
        return X64Emitir(c, MI2(FX_MOV64_RR, destino, fonte));
    case FX_MOV32_RM:
    case FX_MOVZX32_RM8:
    case FX_MOVZX32_RM16:
        return X64Emitir(c, MI2(FX_MOV32_RR, destino, MReg(o->reg, 32)));
    case FX_MOVSX64_RM8:
        return X64Emitir(c, MI2(FX_MOVSX32_RR8, destino32, MReg(o->reg, 8))) && X64Emitir(c, MI2(FX_MOVSXD64_RR32, destino, destino32));
    case FX_MOVSX64_RM16:
        return X64Emitir(c, MI2(FX_MOVSX32_RR16, destino32, MReg(o->reg, 16))) && X64Emitir(c, MI2(FX_MOVSXD64_RR32, destino, destino32));
    default:
        return X64SemRegra(c, "vreg", "forma de leitura sem versao em registrador");
    }
}

/* Escrita MR em vreg em registrador: I32 e I64 gravam com mov (o de 32 bits zera a metade alta); I8 e I16 com movzx, que refaz a extensao com zero. */
static bool AlocGravarEmRegistrador(X64Ctx *c, uint16_t forma, const AlocOperando *o, MOperando origem)
{
    switch (forma) {
    case FX_MOVSS_MX:
    case FX_MOVSD_MX:
        return X64Emitir(c, MI2(FX_MOVAPS_XX, MXmm((uint8_t)o->reg), origem));
    case FX_MOVUPS_MX: /* vetor V128 (S2) */
        return X64Emitir(c, MI2(FX_MOVAPS_XX, MXmm((uint8_t)o->reg), origem));
    case FX_MOV64_MR:
        return X64Emitir(c, MI2(FX_MOV64_RR, MReg(o->reg, 64), origem));
    case FX_MOV32_MR:
        return X64Emitir(c, MI2(FX_MOV32_RR, MReg(o->reg, 32), origem));
    case FX_MOV16_MR:
        return X64Emitir(c, MI2(FX_MOVZX32_RR16, MReg(o->reg, 32), origem));
    case FX_MOV8_MR:
        return X64Emitir(c, MI2(FX_MOVZX32_RR8, MReg(o->reg, 32), origem));
    default:
        return X64SemRegra(c, "vreg", "forma de escrita sem versao em registrador");
    }
}

bool AlocCarregar(X64Ctx *c, uint16_t forma, MOperando destino, const AlocOperando *o)
{
    if (o->em_reg)
        return AlocCarregarDeRegistrador(c, forma, destino, o);
    return X64Emitir(c, MI2(forma, destino, AlocMemoria(o)));
}

bool AlocGravar(X64Ctx *c, uint16_t forma, const AlocOperando *o, MOperando origem)
{
    if (o->em_reg)
        return AlocGravarEmRegistrador(c, forma, o, origem);
    return X64Emitir(c, MI2(forma, AlocMemoria(o), origem));
}

/* Mascara (bit = X64Reg) dos callee-saved que o alocador usa na funcao (passe `alocador`); o prologo os empurra e o epilogo os desempilha. */
uint32_t AlocRegsSalvos(const BirFuncao *f)
{
    return f->regs_salvos;
}

/* Mascara (bit = numero do xmm) dos xmm6..15 que o alocador usa; o prologo os guarda com movups e o epilogo os restaura. */
uint32_t AlocRegsSalvosXmm(const BirFuncao *f)
{
    return f->regs_salvos_xmm;
}

uint32_t AlocBytesSlots(const BirFuncao *f)
{
    return AlocBytesSlotsEscalares(f) + X64_VETOR_BYTES * AlocVetoresAte(f, f->nvregs);
}

bool AlocClassesSuportadas(const X64Ctx *c)
{
    for (uint32_t k = 0; k < c->f->nvregs; k++) {
        if (AlocBits(c->f->classe_vreg[k]) == 0)
            return X64SemRegra(c, "vreg", "classe de vreg fora do M2 (so i8, i16, i32, i64, f32, f64 e v128)");
    }
    return true;
}
