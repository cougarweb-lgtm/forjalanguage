/* x64_abi_win.c - ABI Windows x64 (a unica do M1 e do M2) para chamadas, parametros e retorno: inteiros, reais e agregados por valor.
 * Versao: 0.3.3
 * Changelog: 0.3.3 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.3.2 - M3 F9b-1: decisoes extraidas de AbiWinClassificar.
 *            0.3.1 - M3 P21v-c: AbiWinOcultoDaChamada vale tambem para o CHAMA_IND.
 *            0.3.0 - M2 P11b: AbiArg {classe, tam}; AbiWinClassificar(const AbiArg*, n, AbiLocal*) e agregado por valor (tam 1, 2, 4 ou 8 = os bytes no registrador inteiro da posicao ou no slot
 *                    de pilha; outro tam = o PONTEIRO para a copia, tambem inteiro; nunca xmm); AbiWinRetorno (RAX, XMM0, OCULTO, VAZIO) e os auxiliares do chamado (retorno oculto desloca
 *                    os parametros uma posicao; parametro agregado de 1/2/4/8 bytes ganha um slot de ponteiro).
 *            0.2.0 - M2 P10b: F32 e F64 como argumento: xmm<k> por posicao (k < 4) ou a pilha [rsp+32+8(k-4)]; o retorno real fica em xmm0.
 *            0.1.1 - M2 P9: comentario de ClasseInteira (I16 ja era aceita; o que faltava era a alocacao de 16 bits).
 *            0.1.0 - backend x64 P7a: AbiWinClassificar (RCX, RDX, R8, R9 e pilha a partir de [rsp+32]) e AbiWinParamRbp; retorno em RAX.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/abi
 */
#include "nativo/x64/x64_interno.h"

static const X64Reg REGS_ARGUMENTO[ABI_WIN_REGS_ARG] = {X64_RCX, X64_RDX, X64_R8, X64_R9};

/* Registrador de inteiro de 1 a 8 bytes (I8, I16, I32 e I64). */
static bool ClasseInteira(BirClasse classe)
{
    return classe == BC_I8 || classe == BC_I16 || classe == BC_I32 || classe == BC_I64;
}

static bool ClasseReal(BirClasse classe)
{
    return BirClasseEhReal(classe);
}

/* Agregado de 1, 2, 4 ou 8 bytes: viaja nos proprios bytes (o resto vai por ponteiro). */
bool AbiWinTamEmRegistro(uint32_t tam)
{
    return tam == 1 || tam == 2 || tam == 4 || tam == 8;
}

static bool ArgumentoClassificavel(const AbiArg *a)
{
    if (a->tam > 0)
        return a->classe == BC_I64; /* o agregado e sempre o vreg do endereco */
    return ClasseInteira(a->classe) || ClasseReal(a->classe);
}

/* Onde o argumento da posicao k chega: registrador (k < 4) ou pilha, e quantos bytes ele ocupa se for agregado de 1/2/4/8. */
static AbiLocal AbiWinLocalDoArgumento(const AbiArg *a, uint32_t k)
{
    bool real = a->tam == 0 && ClasseReal(a->classe);
    AbiLocal local;

    if (k < ABI_WIN_REGS_ARG)
        local = (AbiLocal){.na_pilha = false, .reg = real ? (X64Reg)k : REGS_ARGUMENTO[k], .desloc = 0};
    else
        local = (AbiLocal){.na_pilha = true, .reg = X64_RAX, .desloc = (int32_t)(ABI_WIN_SOMBRA + ABI_WIN_BYTES_ARG * (k - ABI_WIN_REGS_ARG))};
    local.bytes = AbiWinTamEmRegistro(a->tam) ? (uint8_t)a->tam : 0;
    return local;
}

/* O registrador do argumento k < 4: RCX, RDX, R8, R9 e, se real, o xmm da MESMA posicao (f(int, f32, int) = rcx, xmm1, r8). Agregado: sempre o inteiro da posicao. */
bool AbiWinClassificar(const AbiArg *args, uint32_t n, AbiLocal *saida)
{
    for (uint32_t k = 0; k < n; k++) {
        if (!ArgumentoClassificavel(&args[k]))
            return false;
        saida[k] = AbiWinLocalDoArgumento(&args[k], k);
    }
    return true;
}

AbiRetorno AbiWinRetorno(BirClasse classe, uint32_t tam)
{
    if (classe == BC_NADA)
        return ABI_RET_VAZIO;
    if (tam > 0)
        return AbiWinTamEmRegistro(tam) ? ABI_RET_RAX : ABI_RET_OCULTO;
    return ClasseReal(classe) ? ABI_RET_XMM0 : ABI_RET_RAX;
}

/* [rbp+0] = rbp salvo, [rbp+8] = retorno, [rbp+16] = home de RCX: os 4 primeiros e os demais ficam em sequencia de 8 em 8. k e a POSICAO (o ponteiro oculto ocupa a 0). */
int32_t AbiWinParamRbp(uint32_t k)
{
    return (int32_t)(16u + ABI_WIN_BYTES_ARG * k);
}

/* Lado do chamador: 1 se o CHAMA devolve um agregado por ponteiro oculto (CHAMA.a = endereco de destino com tam; os argumentos visiveis andam uma posicao). */
uint32_t AbiWinOcultoDaChamada(const BirInstr *i)
{
    bool agregado = (i->op == BIR_CHAMA || i->op == BIR_CHAMA_IND) && i->a.tipo == BO_VREG && i->a.tam != 0;

    return agregado && AbiWinRetorno(BC_I64, i->a.tam) == ABI_RET_OCULTO ? 1u : 0u;
}

/* ---- lado do chamado ---- */

uint32_t AbiWinOculto(const BirFuncao *f)
{
    return AbiWinRetorno((BirClasse)f->retorno, f->retorno_tam) == ABI_RET_OCULTO ? 1u : 0u;
}

bool AbiWinParamEmBytes(const BirFuncao *f, uint32_t k)
{
    return f->param_tam != NULL && AbiWinTamEmRegistro(f->param_tam[k]);
}

uint32_t AbiWinOrdemEmBytes(const BirFuncao *f, uint32_t k)
{
    uint32_t ordem = 0;

    for (uint32_t j = 0; j < k; j++)
        ordem += AbiWinParamEmBytes(f, j) ? 1u : 0u;
    return ordem;
}
