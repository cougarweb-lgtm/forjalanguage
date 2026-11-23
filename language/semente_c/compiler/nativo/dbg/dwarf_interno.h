/* dwarf_interno.h - contexto e auxiliares comuns aos arquivos do DWARF (dwarf_escrever.c, dwarf_tipos.c, dwarf_linhas.c, dwarf_frame.c). Interno: so o dwarf.h e publico.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6b P76d: DwarfCtx ganha simb_proc, dica e o cache do ultimo arquivo; DwarfRelocarProc.
 *            0.2.0 - M6 P67: codigos de abreviacao (enum DWARF_ABREV_*), DwarfAbrev, registro de DIEs de tipo (DwarfTipos: 1 DIE por TipoId, ref4 corrigida no fim) e
 *                    Arena e Layouts no contexto; DwarfRefTipo e DwarfEscreverTipos para dwarf_tipos.c.
 *            0.1.0 - M6 P66: primeira versao (contexto DwarfCtx, tabela de arquivos 1-based e escritores de bytes little-endian/LEB128).
 */
#ifndef FORJA_NATIVO_DBG_DWARF_INTERNO_H
#define FORJA_NATIVO_DBG_DWARF_INTERNO_H

#include "common/arena.h"
#include "ir/layout.h"
#include "nativo/dbg/dwarf.h"

/* Codigos de abreviacao: a ordem e a da tabela ABREVIACOES de dwarf_escrever.c (codigo = posicao + 1). */
enum {
    DWARF_ABREV_CU = 1, DWARF_ABREV_SUB, DWARF_ABREV_PARAM, DWARF_ABREV_VAR, DWARF_ABREV_BASE, DWARF_ABREV_PONTEIRO, DWARF_ABREV_PONTEIRO_VOID,
    DWARF_ABREV_ESTRUTURA, DWARF_ABREV_MEMBRO, DWARF_ABREV_VETOR, DWARF_ABREV_INTERVALO, DWARF_ABREV_ENUM, DWARF_ABREV_ENUMERADOR, DWARF_ABREV_QTD
};

typedef struct { uint64_t codigo, tag; uint8_t filhos; const uint8_t *pares; size_t qtd; } DwarfAbrev;

/* Item do registro de tipos: um DIE de tipo (TIPO), um ponteiro para o tipo id (PONTEIRO) ou o ponteiro sem tipo (VOID, void *). */
enum { DWARF_ITEM_TIPO, DWARF_ITEM_PONTEIRO, DWARF_ITEM_VOID };
typedef struct { uint8_t tipo; TipoId id; } DwarfItem;

/* Uma referencia ref4 do .debug_info: a posicao a corrigir e o indice do item em DwarfTipos.off. */
typedef struct { size_t pos; uint32_t indice; } DwarfRef;

#define DWARF_SEM_DIE UINT32_MAX

/* Registro dos DIE de tipo: off e na_fila tem 2 * ntipos + 1 entradas (TIPO em [0, n), PONTEIRO em [n, 2n), VOID em 2n). */
typedef struct {
    uint32_t ntipos;
    uint32_t *off;           /* offset do DIE no .debug_info; DWARF_SEM_DIE = ainda nao emitido */
    uint8_t *na_fila;        /* 1 = ja enfileirado para emissao */
    Vec fila;                /* DwarfItem, a emitir (cresce enquanto a fila anda) */
    Vec refs;                /* DwarfRef, corrigidas em DwarfPatchRefs */
} DwarfTipos;

/* Contexto da escrita: o objeto, o registro, a semantica, o layout, a arena, os indices das 4 secoes e a tabela de arquivos (indice + 1 = numero no DWARF). */
typedef struct {
    Objeto *o;
    const DbgPrograma *d;
    const DwarfSemantica *s;
    const Fonte *f;          /* = s->fonte */
    Layouts lay;             /* tamanho, alinhamento e offset dos membros (layout/): so le, nunca escreve */
    Arena arena;             /* nomes de arquivo normalizados, tabelas de DwarfTipos e o Layouts */
    uint32_t sec_abrev, sec_info, sec_linha, sec_frame;
    Vec arquivos;            /* const char *: caminho do trecho com barra normal, na ordem da 1a aparicao */
    Vec simb_proc;           /* uint32_t por proc: indice do simbolo no Objeto + 1 (0 = ainda nao achado); .debug_info e .debug_line dividem a busca */
    uint32_t dica;           /* linha (0-based) da ultima posicao resolvida: DbgPosicaoComDica */
    const char *ult_cru;     /* caminho cru e numero DWARF do ultimo arquivo achado: poupa a comparacao do caminho a cada linha */
    uint32_t ult_arquivo;
    DwarfTipos tipos;
} DwarfCtx;

/* Escritores de bytes (little-endian; LEB128 para uleb/sleb). */
void DwarfBytesU8(Buf *b, uint8_t v);
void DwarfBytesU16(Buf *b, uint16_t v);
void DwarfBytesU32(Buf *b, uint32_t v);
void DwarfBytesU64(Buf *b, uint64_t v);
void DwarfBytesUleb(Buf *b, uint64_t v);
void DwarfBytesSleb(Buf *b, int64_t v);
void DwarfBytesTexto(Buf *b, const char *s);
void DwarfPatchU32(Buf *b, size_t pos, uint32_t v);

/* Buffer da secao de indice idx do objeto. Obter de novo depois de abrir secoes (o vetor pode crescer). */
Buf *DwarfBufSecao(const DwarfCtx *x, uint32_t idx);

/* Arquivo e linha de uma posicao da fonte; registra o arquivo na tabela. arquivo = numero 1-based no DWARF. */
uint32_t DwarfArquivoDe(DwarfCtx *x, uint32_t pos, uint32_t *linha);

/* Reloca os 8 bytes em (secao, off) para o endereco do inicio do simbolo (ADDR64, adendo 0; o ligador escreve o valor). */
void DwarfRelocar(DwarfCtx *x, uint32_t secao, size_t off, const char *simbolo);

/* Como DwarfRelocar, para o simbolo do proc de indice 'proc' (a busca pelo nome sai uma vez por proc, nao uma por secao). */
void DwarfRelocarProc(DwarfCtx *x, uint32_t secao, size_t off, size_t proc);

/* Tipos (dwarf_tipos.c): ref4 ao DIE do tipo id no .debug_info (enfileira o DIE se ainda nao saiu); ao fim, DwarfEscreverTipos emite a fila
 * como filhos da CU e DwarfPatchRefs corrige as referencias. */
void DwarfRefTipo(DwarfCtx *x, Buf *b, TipoId id);
void DwarfTiposIniciar(DwarfCtx *x);
void DwarfEscreverTipos(DwarfCtx *x);
void DwarfPatchRefs(DwarfCtx *x);

/* Secoes de linha e de CFI (dwarf_linhas.c e dwarf_frame.c): leem a tabela de procs e de linhas do registro. */
void DwarfEscreverLinhas(DwarfCtx *x);
void DwarfEscreverFrames(DwarfCtx *x);

#endif
