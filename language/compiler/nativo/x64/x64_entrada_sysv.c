/* x64_entrada_sysv.c - o stub fjt_<proc> de um callback escalar no alvo x86_64-linux (M5b P38b): o C chama pela ABI System V, a Forja fala Win64.
 * O stub nao e BirFuncao (nao entra no .pdata): guarda rdi, rsi, rdx, rcx, r8, r9 e xmm0..7 no bloco do quadro, chama o wrapper fjtw_<proc> (BirFuncao Win64,
 * rcx = bloco, rdx = argumentos de pilha do chamador C) e devolve rax, rdx, xmm0 e xmm1 lidos do bloco. Nao toca rbx, r12..r15 (SysV e Win64 os preservam).
 * Quadro: push rbp / mov rbp,rsp / sub rsp,176 = 32 de sombra Win64 + ABI_SYSV_BLOCO_BYTES; rsp fica multiplo de 16 no call (na entrada vale 8 mod 16).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P38b: entrada Win64 (jmp fjtm_<proc>) 16 bytes antes do stub quando o programa passa comparador ao runtime (BirEntradaSysv.win64).
 *            0.1.0 - M5b P38b: primeira versao (X64GerarEntradasSysv, uma entrada por BirPrograma.entradas_sysv).
 * Verificacao: tests\nativo\linux\abi_sysv\verificar_abi_sysv_cb.py --forja <copia do forja.exe> (callback SysV no alpine-build).
 */
#include "nativo/bir/x64_abi_sysv.h"
#include "nativo/x64/x64_interno.h"

#define ENTRADA_FRAME (ABI_WIN_SOMBRA + ABI_SYSV_BLOCO_BYTES) /* sombra Win64 + bloco: multiplo de 16 */
#define ENTRADA_ARGS_PILHA 16                                 /* [rbp+16]: depois do rbp salvo e do endereco de retorno, o 1o argumento de pilha do C */
#define ENTRADA_XMM_ARG 8u

typedef struct {
    X64Reg x64;
    AbiSysvReg abi;
} RegArgumento;

static const RegArgumento REGS_ARGUMENTO[ABI_SYSV_REGS_INT] = {
    {X64_RDI, ABI_SYSV_RDI}, {X64_RSI, ABI_SYSV_RSI}, {X64_RDX, ABI_SYSV_RDX}, {X64_RCX, ABI_SYSV_RCX}, {X64_R8, ABI_SYSV_R8}, {X64_R9, ABI_SYSV_R9},
};

/* O bloco mora logo acima da sombra de 32 bytes que o wrapper (Win64) espera em [rsp]. */
static MOperando NoBloco(uint32_t desloc)
{
    return MMem(X64_RSP, (int32_t)(ABI_WIN_SOMBRA + desloc), 64);
}

static bool GuardarArgumentos(X64Ctx *c)
{
    for (uint32_t k = 0; k < ABI_SYSV_REGS_INT; k++) {
        if (!X64Emitir(c, MI2(FX_MOV64_MR, NoBloco(AbiSysvBlocoArg(REGS_ARGUMENTO[k].abi)), MReg(REGS_ARGUMENTO[k].x64, 64))))
            return false;
    }
    for (uint32_t j = 0; j < ENTRADA_XMM_ARG; j++) {
        if (!X64Emitir(c, MI2(FX_MOVSD_MX, NoBloco(AbiSysvBlocoArg((AbiSysvReg)(ABI_SYSV_XMM0 + j))), MXmm((uint8_t)j))))
            return false;
    }
    return true;
}

/* rcx = bloco, rdx = argumentos de pilha do chamador C, e o call ao wrapper (relocacao rel32 como a de qualquer proc). */
static bool ChamarWrapper(X64Ctx *c, const BirEntradaSysv *entrada)
{
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RCX, 64), NoBloco(0))) &&
           X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RDX, 64), MMem(X64_RBP, ENTRADA_ARGS_PILHA, 64))) &&
           X64Emitir(c, MI1(FX_CALL_REL32, MSimbolo(c->obj_sim[entrada->wrapper])));
}

/* O retorno do wrapper volta nos 4 registradores de retorno da ABI; o que o proc nao devolveu fica com o lixo do bloco, que o chamador C ignora. */
static bool RecarregarRetorno(X64Ctx *c)
{
    const MInstr lista[] = {
        MI2(FX_MOV64_RM, MReg(X64_RAX, 64), NoBloco(AbiSysvBlocoRet(ABI_SYSV_RAX))),
        MI2(FX_MOV64_RM, MReg(X64_RDX, 64), NoBloco(AbiSysvBlocoRet(ABI_SYSV_RDX))),
        MI2(FX_MOVSD_XM, MXmm(0), NoBloco(AbiSysvBlocoRet(ABI_SYSV_XMM0))),
        MI2(FX_MOVSD_XM, MXmm(1), NoBloco(AbiSysvBlocoRet(ABI_SYSV_XMM1))),
    };

    for (uint32_t k = 0; k < 4; k++) {
        if (!X64Emitir(c, lista[k]))
            return false;
    }
    return true;
}

/* Entrada Win64 do stub (o runtime, que fala Win64, chama o comparador de sort_by/heap nela): "jmp fjtm_<proc>" + int3 ate 16 bytes, logo antes do fjt_ (alinhado a 16),
 * entao fjt_ - ABI_SYSV_ENTRADA_WIN64_ANTES e o endereco dela. O jmp rel32 sai em bytes crus (FX_JMP_REL32 so aceita bloco local) com a relocacao que o X64Emitir poria. */
static void EmitirEntradaWin64(X64Ctx *c, const BirEntradaSysv *entrada)
{
    static const char JMP_REL32[5] = {(char)0xE9, 0, 0, 0, 0};
    ObjSecao *texto = VecEm(&c->o->secoes, c->texto);

    X64Padding(c);
    BufAnexarN(&texto->bytes, JMP_REL32, sizeof JMP_REL32);
    ObjRelocar(c->o, (ObjAlvoReloc){c->texto, X64Offset(c) - 4u}, (ObjCorrecao){OR_REL32, c->obj_sim[entrada->win64], 0});
    X64Padding(c);
}

static bool GerarEntrada(X64Ctx *c, const BirEntradaSysv *entrada)
{
    bool ok;

    if (entrada->win64 != BIR_SEM_SIMBOLO)
        EmitirEntradaWin64(c, entrada);
    ObjSimboloDefinir(c->o, c->p->simbolos[entrada->stub].nome, (int32_t)c->texto, X64Offset(c));
    ok = X64Emitir(c, MI1(FX_PUSH64_R, MReg(X64_RBP, 64))) && X64Emitir(c, MI2(FX_MOV64_RR, MReg(X64_RBP, 64), MReg(X64_RSP, 64))) &&
         X64Emitir(c, MI2(FX_SUB64_RI, MReg(X64_RSP, 64), MImed(ENTRADA_FRAME))) && GuardarArgumentos(c) && ChamarWrapper(c, entrada) &&
         RecarregarRetorno(c) && X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RSP, 64), MMem(X64_RBP, 0, 64))) &&
         X64Emitir(c, MI1(FX_POP64_R, MReg(X64_RBP, 64))) && X64Emitir(c, MI0(FX_RET));
    X64Padding(c);
    return ok;
}

/* Sem listagem (--debug-code) e sem funcao corrente: o stub nao e BirFuncao. */
bool X64GerarEntradasSysv(X64Ctx *c)
{
    AlvoListagem *lista = c->lista;
    bool ok = true;

    c->f = NULL;
    c->instr = NULL;
    c->lista = NULL;
    for (uint32_t k = 0; ok && k < c->p->nentradas_sysv; k++)
        ok = GerarEntrada(c, &c->p->entradas_sysv[k]);
    c->lista = lista;
    return ok;
}
