/* x64_isel_mem.c - selecao das operacoes de memoria da BIR (END_SLOT, CARREGA, GRAVA, COPIA_MEM, ZERA_MEM) no degrau 0.
 * Versao: 0.3.2
 * Changelog: 0.3.2 - S2 (D0703): CARREGA e GRAVA de V128 vao para IselCarregaVetor e IselGravaVetor (movups).
 *            0.3.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.3.0 - M5b P39-B1 (D0654): IselBlocoMem chama o nome de copia de BirPrograma.nomes_mem (linux: forja_memcpy), no lugar do texto fixo.
 *            0.2.0 - M2 P11b: IselLerBytes, IselGravarBytes e IselBlocoMem (copia de n bytes, com call memcpy do runtime acima de 64) para o agregado por valor; PecasDoBloco recebe n.
 *            0.1.0 - M2 P11a: END_SLOT = lea rax,[rbp-off] (off de X64Ctx.slot_off); CARREGA/GRAVA com a base em rcx e o valor em rax (inteiro) ou xmm0 (real); I8 e I16 carregam por
 *                    movzx da largura (o EXT_S/EXT_Z alarga depois). COPIA_MEM e ZERA_MEM em movs de 8 bytes pelo rax (rcx = destino, rdx = origem) com a cauda de 4, 2 e 1 byte;
 *                    nunca rep movs nem rdi/rsi (preservados no Win64); o construtor so cria essas duas ate 64 bytes.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/struct
 *
 * Rascunho: rax, rcx, rdx e xmm0. Nada e preservado entre duas BirInstr.
 */
#include "nativo/x64/x64_interno.h"

bool IselEndSlot(X64Ctx *c, const BirInstr *i)
{
    MOperando slot = MMem(X64_RBP, -(int32_t)c->slot_off[i->a.imed], 64);

    return X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RAX, 64), slot)) && IselGuardar(c, i->dst, X64_RAX);
}

/* Como o valor de um tamanho de dado entra e sai do rax: forma de carga, forma de gravacao e as larguras do registrador em cada uma. */
typedef struct { uint8_t bytes, largura_mem, bits_carga, bits_grava; uint16_t carga, grava; } DadoMem;

static const DadoMem DADOS_MEM[] = {
    {8, 64, 64, 64, FX_MOV64_RM, FX_MOV64_MR},
    {4, 32, 32, 32, FX_MOV32_RM, FX_MOV32_MR},
    {2, 16, 32, 16, FX_MOVZX32_RM16, FX_MOV16_MR},
    {1, 8, 32, 8, FX_MOVZX32_RM8, FX_MOV8_MR},
};

static const DadoMem *DadoDeBytes(uint32_t bytes)
{
    for (uint32_t k = 0; k < sizeof DADOS_MEM / sizeof DADOS_MEM[0]; k++) {
        if (DADOS_MEM[k].bytes == bytes)
            return &DADOS_MEM[k];
    }
    return NULL;
}

bool IselCarrega(X64Ctx *c, const BirInstr *i)
{
    uint8_t bits = AlocBits(i->classe);
    MOperando origem = MMem(X64_RCX, i->desloc, bits);
    const DadoMem *dado = DadoDeBytes(bits / 8u);
    MInstr carga;

    if (i->classe == BC_V128) /* S2: movups xmm, [rcx+desloc] */
        return IselCarregaVetor(c, i);
    carga = BirClasseEhReal(i->classe) ? MI2(i->classe == BC_F64 ? FX_MOVSD_XM : FX_MOVSS_XM, MXmm(0), origem)
                                       : MI2(dado->carga, MReg(X64_RAX, dado->bits_carga), origem);
    if (!IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA))
        return false;
    return X64Emitir(c, carga) && IselGuardar(c, i->dst, X64_RAX); /* real: X64_RAX vale xmm0 */
}

bool IselGrava(X64Ctx *c, const BirInstr *i)
{
    uint8_t bits = AlocBits(i->b.classe);
    MOperando destino = MMem(X64_RCX, i->desloc, bits);
    const DadoMem *dado = DadoDeBytes(bits / 8u);

    if (i->b.classe == BC_V128) /* S2: movups [rcx+desloc], xmm */
        return IselGravaVetor(c, i);
    if (!IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA) || !IselCarregar(c, X64_RAX, &i->b, BE_NENHUMA))
        return false;
    if (BirClasseEhReal(i->b.classe))
        return IselGuardarXmm(c, i->b.classe, destino, 0);
    return X64Emitir(c, MI2(dado->grava, destino, MReg(X64_RAX, dado->bits_grava)));
}

/* Bloco de n bytes em pedacos de 8, 4, 2 e 1: cada um e carregado de [rdx+off] no rax (so na copia; a zeragem usa o rax zerado) e gravado em [rcx+off]. */
static bool PecasDoBloco(X64Ctx *c, uint32_t n, bool copia)
{
    uint32_t resta = n, off = 0;

    for (uint32_t k = 0; k < sizeof DADOS_MEM / sizeof DADOS_MEM[0]; k++) {
        const DadoMem *d = &DADOS_MEM[k];

        for (; resta >= d->bytes; resta -= d->bytes, off += d->bytes) {
            if (copia && !X64Emitir(c, MI2(d->carga, MReg(X64_RAX, d->bits_carga), MMem(X64_RDX, (int32_t)off, d->largura_mem))))
                return false;
            if (!X64Emitir(c, MI2(d->grava, MMem(X64_RCX, (int32_t)off, d->largura_mem), MReg(X64_RAX, d->bits_grava))))
                return false;
        }
    }
    return true;
}

bool IselCopiaMem(X64Ctx *c, const BirInstr *i)
{
    if (!IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA) || !IselCarregar(c, X64_RDX, &i->b, BE_NENHUMA))
        return false;
    return PecasDoBloco(c, (uint32_t)i->desloc, true);
}

bool IselZeraMem(X64Ctx *c, const BirInstr *i)
{
    if (!IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA))
        return false;
    return X64Emitir(c, MI2(FX_XOR32_RR, MReg(X64_RAX, 32), MReg(X64_RAX, 32))) && PecasDoBloco(c, (uint32_t)i->desloc, false);
}

/* ---- agregado por valor (P11b) ---- */

bool IselLerBytes(X64Ctx *c, X64Reg reg, X64Reg base, uint32_t bytes)
{
    const DadoMem *dado = DadoDeBytes(bytes);

    if (dado == NULL)
        return X64SemRegra(c, "agregado", "leitura de agregado que nao tem 1, 2, 4 ou 8 bytes");
    return X64Emitir(c, MI2(dado->carga, MReg(reg, dado->bits_carga), MMem(base, 0, dado->largura_mem)));
}

bool IselGravarBytes(X64Ctx *c, X64Reg base, X64Reg reg, uint32_t bytes)
{
    const DadoMem *dado = DadoDeBytes(bytes);

    if (dado == NULL)
        return X64SemRegra(c, "agregado", "gravacao de agregado que nao tem 1, 2, 4 ou 8 bytes");
    return X64Emitir(c, MI2(dado->grava, MMem(base, 0, dado->largura_mem), MReg(reg, dado->bits_grava)));
}

/* rcx = destino, rdx = origem. Acima do limite da BIR o runtime copia: copia(rcx, rdx, r8d), com o nome do alvo (BirPrograma.nomes_mem). */
bool IselBlocoMem(X64Ctx *c, uint32_t n)
{
    if (n <= BIR_COPIA_MEM_MAX)
        return PecasDoBloco(c, n, true);
    if (!X64Emitir(c, MI2(FX_MOV32_RI, MReg(X64_R8, 32), MImed((int32_t)n))))
        return false;
    return X64Emitir(c, MI1(FX_CALL_REL32, MSimbolo(ObjSimboloReferir(c->o, c->p->nomes_mem.copia))));
}
