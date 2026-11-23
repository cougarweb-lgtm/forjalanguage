/* dwarf_linhas.c - .debug_line DWARF 4 do programa (M6 P66, D-M6-3): cabecalho com a tabela de arquivos e, por proc, uma sequencia que liga cada
 * linha do registro (DbgLinha: offset em .text, pos na fonte) ao endereco. Uma linha da tabela = um DbgLinha (o colapso de repeticoes e do leitor).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P76d: relocacao pelo indice do proc (DwarfRelocarProc) e corpo reservado de uma vez; saida igual.
 *            0.1.0 - M6 P66: primeira versao (linha v4, sequencia por proc com DW_LNE_set_address relocado a simbolo do proc, end_sequence).
 * Verificacao: construir.bat && python ..\tests\nativo\m6\verificar_dwarf.py --forja .\forja.exe --alvo linux
 */
#include <string.h>

#include "nativo/dbg/dwarf_interno.h"

#define DWARF_VERSAO_LINHA 4
#define DWARF_LINHA_BASE (-5)
#define DWARF_LINHA_RANGE 14
#define DWARF_OPCODE_BASE 13
#define DW_LNS_COPY 1
#define DW_LNS_ADVANCE_PC 2
#define DW_LNS_ADVANCE_LINE 3
#define DW_LNS_SET_FILE 4
#define DW_LNE_END_SEQUENCE 1
#define DW_LNE_SET_ADDRESS 2
#define DW_LNE_TAM_SET_ADDRESS 9 /* sub-opcode (1) + endereco de 8 bytes */

#define DWARF_BYTES_POR_PROC 24   /* estimativa para reservar o corpo de uma vez (set_address 12 + end_sequence 3 + avancos); so desempenho, nao muda a saida */
#define DWARF_BYTES_POR_LINHA 4

/* Numero de operandos ULEB de cada opcode padrao 1..12 (o DWARF 4 do x86-64 que o gcc tambem usa). */
static const uint8_t DWARF_ARGS_OPCODE[12] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};

/* Relocacao pendente: o campo do endereco esta no corpo da sequencia, em off; so ganha a posicao final depois do cabecalho. */
typedef struct { size_t off, proc; } DwarfPendente;

static void DwarfCabecalhoLinhas(DwarfCtx *x, Buf *b)
{
    size_t hdr_pos;

    DwarfBytesU16(b, DWARF_VERSAO_LINHA);
    hdr_pos = b->tam;
    DwarfBytesU32(b, 0); /* header_length: preenchido ao fim do cabecalho */
    DwarfBytesU8(b, 1);  /* minimum_instruction_length */
    DwarfBytesU8(b, 1);  /* maximum_operations_per_instruction */
    DwarfBytesU8(b, 1);  /* default_is_stmt */
    DwarfBytesU8(b, (uint8_t)(int8_t)DWARF_LINHA_BASE);
    DwarfBytesU8(b, DWARF_LINHA_RANGE);
    DwarfBytesU8(b, DWARF_OPCODE_BASE);
    for (size_t k = 0; k < sizeof DWARF_ARGS_OPCODE; k++)
        DwarfBytesU8(b, DWARF_ARGS_OPCODE[k]);
    DwarfBytesU8(b, 0); /* include_directories: vazio (os caminhos vao completos) */
    for (size_t i = 0; i < x->arquivos.n; i++) {
        DwarfBytesTexto(b, *(const char **)VecEm(&x->arquivos, i));
        DwarfBytesUleb(b, 0); /* diretorio */
        DwarfBytesUleb(b, 0); /* mtime */
        DwarfBytesUleb(b, 0); /* tamanho */
    }
    DwarfBytesU8(b, 0); /* fim da tabela de arquivos */
    DwarfPatchU32(b, hdr_pos, (uint32_t)(b->tam - hdr_pos - 4));
}

static void DwarfAvancaPc(Buf *corpo, uint64_t delta)
{
    DwarfBytesU8(corpo, DW_LNS_ADVANCE_PC);
    DwarfBytesUleb(corpo, delta);
}

static void DwarfLinhasDoProc(DwarfCtx *x, Buf *corpo, Vec *pend, size_t indice)
{
    const DbgProc *p = VecEm(&x->d->procs, indice);
    uint32_t arquivo = 1, linha = 1, atual = p->inicio;
    DwarfPendente *q;

    DwarfBytesU8(corpo, 0); /* extended opcode */
    DwarfBytesUleb(corpo, DW_LNE_TAM_SET_ADDRESS);
    DwarfBytesU8(corpo, DW_LNE_SET_ADDRESS);
    q = VecEmpurrar(pend);
    q->off = corpo->tam;
    q->proc = indice;
    DwarfBytesU64(corpo, 0);
    for (uint32_t k = p->linha_ini; k < p->linha_ini + p->nlinhas; k++) {
        const DbgLinha *l = VecEm(&x->d->linhas, k);
        uint32_t nova_linha, nova_arq = DwarfArquivoDe(x, l->pos, &nova_linha);

        if (nova_arq != arquivo) {
            DwarfBytesU8(corpo, DW_LNS_SET_FILE);
            DwarfBytesUleb(corpo, nova_arq);
            arquivo = nova_arq;
        }
        if (l->off > atual) {
            DwarfAvancaPc(corpo, l->off - atual);
            atual = l->off;
        }
        if (nova_linha != linha) {
            DwarfBytesU8(corpo, DW_LNS_ADVANCE_LINE);
            DwarfBytesSleb(corpo, (int64_t)nova_linha - (int64_t)linha);
            linha = nova_linha;
        }
        DwarfBytesU8(corpo, DW_LNS_COPY);
    }
    if (p->fim > atual)
        DwarfAvancaPc(corpo, p->fim - atual);
    DwarfBytesU8(corpo, 0); /* extended opcode: end_sequence */
    DwarfBytesUleb(corpo, 1);
    DwarfBytesU8(corpo, DW_LNE_END_SEQUENCE);
}

void DwarfEscreverLinhas(DwarfCtx *x)
{
    Buf *sec = DwarfBufSecao(x, x->sec_linha);
    Buf corpo = {0};
    Vec pend;
    size_t tam_pos = sec->tam, base;

    VecIniciar(&pend, sizeof(DwarfPendente));
    VecReservar(&pend, x->d->procs.n);
    BufReservarTotal(&corpo, x->d->procs.n * DWARF_BYTES_POR_PROC + x->d->linhas.n * DWARF_BYTES_POR_LINHA);
    for (size_t i = 0; i < x->d->procs.n; i++)
        DwarfLinhasDoProc(x, &corpo, &pend, i);
    DwarfBytesU32(sec, 0); /* unit_length: preenchido no fim */
    DwarfCabecalhoLinhas(x, sec);
    base = sec->tam;
    BufAnexarN(sec, corpo.dados, corpo.tam);
    DwarfPatchU32(sec, tam_pos, (uint32_t)(sec->tam - tam_pos - 4));
    for (size_t i = 0; i < pend.n; i++) {
        const DwarfPendente *q = VecEm(&pend, i);

        DwarfRelocarProc(x, x->sec_linha, base + q->off, q->proc);
    }
    BufLiberar(&corpo);
    VecLiberar(&pend);
}
