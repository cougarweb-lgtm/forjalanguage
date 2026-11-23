/* dwarf_frame.c - .debug_frame DWARF 4 do programa (M6 P66, D-M6-3): um CIE e um FDE por proc fj_. O prologo padrao do backend x64
 * (push rbp; mov rbp, rsp) e o que o CFI descreve; o FDE leva o endereco do proc por relocacao ADDR64 (D-M6-4).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M6 P66: primeira versao (CIE com RA = rip, FDE com push/mov rbp e CFA em rbp; entradas alinhadas a 8).
 * Verificacao: construir.bat && python ..\tests\nativo\m6\verificar_gdb.py --forja .\forja.exe --alvo linux
 */
#include "nativo/dbg/dwarf_interno.h"

#define DWARF_CIE_ID 0xffffffffu
#define DWARF_VERSAO_CFI 1
#define DWARF_CFA_NOP 0x00
#define DWARF_CFA_ADVANCE 0x40    /* DW_CFA_advance_loc (delta no proprio byte, < 64) */
#define DWARF_CFA_OFFSET 0x80     /* DW_CFA_offset (registrador no proprio byte) */
#define DWARF_CFA_DEF_CFA 0x0c
#define DWARF_CFA_DEF_CFA_OFFSET 0x0e
#define DWARF_CFA_DEF_CFA_REG 0x0d
#define DWARF_REG_RBP 6
#define DWARF_REG_RSP 7
#define DWARF_REG_RA 16           /* rip: a coluna do endereco de retorno */
#define DWARF_FATOR_DADOS (-8)    /* um slot de 8 bytes */
#define DWARF_ALINHA_ENTRADA 8
#define DWARF_TAM_PUSH_RBP 1      /* push rbp = 1 byte */
#define DWARF_TAM_MOV_RBP_RSP 3   /* mov rbp, rsp = 3 bytes */
#define DWARF_TAM_CFA_INICIAL 8

/* Completa a entrada com DW_CFA_nop ate o tamanho ser multiplo de 8 (contado desde o campo length). */
static void DwarfAlinharEntrada(Buf *b, size_t inicio)
{
    while ((b->tam - inicio) % DWARF_ALINHA_ENTRADA != 0)
        DwarfBytesU8(b, DWARF_CFA_NOP);
}

static void DwarfEscreverCie(DwarfCtx *x)
{
    Buf *b = DwarfBufSecao(x, x->sec_frame);
    size_t inicio = b->tam;

    DwarfBytesU32(b, 0); /* length: preenchido no fim */
    DwarfBytesU32(b, DWARF_CIE_ID);
    DwarfBytesU8(b, DWARF_VERSAO_CFI);
    DwarfBytesU8(b, 0); /* augmentation vazia */
    DwarfBytesUleb(b, 1); /* code_alignment_factor */
    DwarfBytesSleb(b, DWARF_FATOR_DADOS);
    DwarfBytesU8(b, DWARF_REG_RA);
    DwarfBytesU8(b, DWARF_CFA_DEF_CFA);
    DwarfBytesUleb(b, DWARF_REG_RSP);
    DwarfBytesUleb(b, DWARF_TAM_CFA_INICIAL);
    DwarfBytesU8(b, DWARF_CFA_OFFSET | DWARF_REG_RA);
    DwarfBytesUleb(b, 1); /* rip salvo em CFA-8 */
    DwarfAlinharEntrada(b, inicio);
    DwarfPatchU32(b, inicio, (uint32_t)(b->tam - inicio - 4));
}

static void DwarfEscreverFde(DwarfCtx *x, const DbgProc *p)
{
    Buf *b = DwarfBufSecao(x, x->sec_frame);
    size_t inicio = b->tam;

    DwarfBytesU32(b, 0); /* length: preenchido no fim */
    DwarfBytesU32(b, 0); /* CIE_pointer: o CIE esta no offset 0 */
    DwarfRelocar(x, x->sec_frame, b->tam, p->simbolo); /* initial_location */
    DwarfBytesU64(b, 0);
    DwarfBytesU64(b, p->fim - p->inicio);                  /* address_range */
    DwarfBytesU8(b, DWARF_CFA_ADVANCE | DWARF_TAM_PUSH_RBP);
    DwarfBytesU8(b, DWARF_CFA_DEF_CFA_OFFSET);
    DwarfBytesUleb(b, 2 * DWARF_TAM_CFA_INICIAL);          /* CFA = rsp + 16 depois do push */
    DwarfBytesU8(b, DWARF_CFA_OFFSET | DWARF_REG_RBP);
    DwarfBytesUleb(b, 2);                                  /* rbp salvo em CFA-16 */
    DwarfBytesU8(b, DWARF_CFA_ADVANCE | DWARF_TAM_MOV_RBP_RSP);
    DwarfBytesU8(b, DWARF_CFA_DEF_CFA_REG);
    DwarfBytesUleb(b, DWARF_REG_RBP);                      /* depois do mov: CFA = rbp + 16 */
    DwarfAlinharEntrada(b, inicio);
    DwarfPatchU32(b, inicio, (uint32_t)(b->tam - inicio - 4));
}

void DwarfEscreverFrames(DwarfCtx *x)
{
    DwarfEscreverCie(x);
    for (size_t i = 0; i < x->d->procs.n; i++)
        DwarfEscreverFde(x, VecEm(&x->d->procs, i));
}
