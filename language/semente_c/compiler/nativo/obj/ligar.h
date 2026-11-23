/* ligar.h - ligador do backend nativo: junta o forja_programa e o runtime (objetos relocaveis) numa Imagem PE32+ pronta para PeEscrever.
 * Versao: 0.21.0
 * Changelog: 0.21.0 - M8 P2: LigarPedido.perfil (16o objeto, o runtime do --perfil).
 *            0.20.0 - M5b P41: LigarPedido.pie (--pie: ELF static-pie, base 0, relocacoes RELATIVE em Imagem.relativos; ABS32/ABS32S e TEXTREL viram X0112).
 *            0.19.0 - M5b P39-B2: LigarPedido.semente (15o objeto, crt1.o do musl; o _start da variante libc).
 *            0.18.0 - M5b P39: LigarPedido.entrada (14o objeto, o _start do ELF: rt_linux_entrada_cru.o).
 *            0.17.0 - M5 P53: LigarPedido.net (13o objeto, rt_win_net.o, std:net UDP) e a WS2_32 como 3a tabela fixa so com ele.
 *            0.16.0 - M5 P52: LigarPedido.varena (12o objeto, rt_win_varena.o, std:mem VirtualArena).
 *            0.15.0 - M5 P50: LigarPedido.atomic (11o objeto, rt_win_atomic.o, std:atomic).
 *            0.14.0 - M5 P49: LigarPedido.sync (10o objeto, rt_win_sync.o, std:sync).
 *            0.13.0 - M5 P47: LigarPedido.thread (9o objeto, rt_win_thread.o). 0.12.0 - S3 P5b-1: LigarPedido.es (8o objeto opcional: rt_win_es.o, handle e fs do std:file).
 *            0.11.0 - L11a-2: LigarPedido.math (7o objeto opcional: rt_math.o).
 *            0.10.0 - IA1-L5a: LigarPedido.curto (6o objeto opcional: rt_win_curto.o).
 *            0.9.0 - M5 P27a: LigarPedido.formato (IMAGEM_PE e o zero; IMAGEM_ELF = base 0x400000, entrada _start do runtime, secoes .rdata .text .data .bss, sem imports, .pdata/.xdata, DIR64 nem .reloc, com simbolos na Imagem).
 *            0.8.0 - M3b P23f: LigarPedido.legado (5o objeto opcional: rt_win_legado.o).
 *            0.7.0 - M3b P23e: LigarPedido.terceiros (4o objeto opcional: os membros de bibliotecas estaticas, terceiros.h).
 *            0.6.0 - M3 F5: LigarPedido.ryu (3o objeto opcional: o Ryu, rt_win_ryu.o).
 *            0.5.0 - M3 P22a: LigarPedido.dll (entrada = forja_entrada_dll, ImageBase da DLL nos ADDR64), nome_dll, exports e qtd_exports (a .edata com os simbolos do programa).
 *            0.4.0 - M3 P19: LigarPedido e LigarComTabelas (as DLLs dos 'link' depois da KERNEL32); Ligar continua com 4 parametros e sem tabelas extras.
 *            0.3.0 - M3 P19a: ImportTabela {dll, simbolos}: o ligador aceita __imp_ de uma lista de DLLs (hoje so a KERNEL32 do runtime); LigarImportacoes sai
 *                    (a Imagem guarda as DLLs: ImagemImportacao.dlls).
 *            0.2.0 - M2 P17c: Ligar descarta o que nao e alcancavel (secoes e __imp_); LigarImportacoes recebe a Imagem (so os imports alcancaveis estao no .idata).
 *            0.1.1 - backend x64 P8b: LigarImportacoes.
 *            0.1.0 - backend x64 P5: Ligar (ordem das secoes do PE, __imp_ pela tabela KERNEL32, REL32/ADDR32NB/ADDR64 e entrada).
 * Verificacao: construir.bat && teste_nativo.exe obj <pasta> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#ifndef FORJA_NATIVO_OBJ_LIGAR_H
#define FORJA_NATIVO_OBJ_LIGAR_H

#include "common/diag.h"
#include "nativo/fmt/pe.h"
#include "nativo/obj/objeto.h"

/* Uma DLL e os simbolos dela que o exe aceita importar (slot da IAT = __imp_<simbolo>). A tabela do runtime e a primeira, a KERNEL32; as dos 'link' do
 * programa vem depois, na ordem dos link. As DLLs e os nomes que o exe importa de fato ficam em Imagem.importacao (dlls, nomes, iat_rva). */
typedef struct {
    const char *dll;
    const char *const *simbolos;
    uint32_t qtd;
} ImportTabela;

/* O que LigarComTabelas liga: os dois objetos e as DLLs dos 'link' do programa (tabelas, depois da KERNEL32 do runtime, na ordem dos link; os ponteiros e os
 * nomes devem viver ate ImagemLiberar). Cada tabela lista so os simbolos que o programa usa, na ordem da 1a referencia; um simbolo que ja esta numa tabela anterior
 * fica na anterior (a 1a que exporta vence, como o ld). */
typedef struct {
    const Objeto *programa, *runtime;
    const Objeto *ryu;               /* opcional (NULL = sem): o Ryu do runtime, objeto a parte (rt_win_ryu.o) */
    const Objeto *terceiros;         /* opcional (NULL = sem): a uniao dos membros de bibliotecas estaticas que o programa usa (TerceirosMontar) */
    const Objeto *legado;            /* opcional (NULL = sem): rt_win_legado.o (stubs da toolchain e sprintf/sscanf), so quando um membro de estatica pede um simbolo dele */
    const Objeto *math;              /* opcional (NULL = sem): rt_math.o (libm do musl, std:math), so quando o programa referencia forja_math_* (L11a-2) */
    const Objeto *es;                /* opcional (NULL = sem): rt_win_es.o (handle e fs do std:file), so quando o programa referencia forja_fs_* (S3 P5b-1) */
    const Objeto *thread;            /* opcional (NULL = sem): rt_win_thread.o (std:thread, M5 P47), so quando o programa referencia forja_thread_* */
    const Objeto *sync;              /* opcional (NULL = sem): rt_win_sync.o (std:sync, M5 P49), so quando o programa referencia forja_sync_* */
    const Objeto *atomic;            /* opcional (NULL = sem): rt_win_atomic.o (std:atomic, M5 P50), so quando o programa referencia forja_atomic_* */
    const Objeto *varena;            /* opcional (NULL = sem): rt_win_varena.o (std:mem VirtualArena, M5 P52), so quando o programa referencia forja_varena_* */
    const Objeto *net;               /* opcional (NULL = sem): rt_win_net.o (std:net UDP, M5 P53), so quando o programa referencia forja_net_*; leva a WS2_32 como 3a tabela fixa */
    const Objeto *curto;             /* opcional (NULL = sem): rt_win_curto.o (print curto de real e eprint), so quando o programa referencia um simbolo dele (IA1-L5a) */
    const Objeto *entrada;           /* opcional (NULL = sem): o objeto de entrada do ELF (rt_linux_entrada_cru.o, o _start; M5b P39; na variante libc, rt_linux_entrada_libc.o, o main). O ELF exige; o PE nao usa */
    const Objeto *semente;           /* opcional (NULL = sem): crt1.o do musl (M5b P39-B2, D-R4): o _start da variante libc; com ele a raiz da entrada e o _start dele */
    const Objeto *perfil;            /* opcional (NULL = sem): rt_win_perfil.o / rt_linux_perfil.o (o runtime do --perfil, M8 P2); so com --perfil, e o programa define forja_perfil_tabela */
    const ImportTabela *tabelas;
    uint32_t qtd_tabelas;
    bool dll;                        /* DLL: o ponto de entrada e o forja_entrada_dll do runtime e a ImageBase e a da DLL */
    const char *nome_dll;            /* so com dll: o nome do arquivo (o campo Name do export directory) */
    const char *const *exports;      /* so com dll: os nomes (definidos globais no programa) que a .edata exporta, em qualquer ordem; vivem ate ImagemLiberar */
    uint32_t qtd_exports;
    ImagemFormato formato;           /* IMAGEM_PE (0) ou IMAGEM_ELF: o ELF nao aceita tabelas de imports nem DLL (X0903) e descarta o .pdata/.xdata do programa */
    bool pie;                        /* ELF static-pie (--pie, P41): so com IMAGEM_ELF; a semente entra como rcrt1.o e o ligador define _DYNAMIC */
} LigarPedido;

/* Igual a Ligar, com as tabelas de imports dos 'link' do programa. Mais de IMAGEM_MAX_DLLS tabelas no total ou de IMAGEM_MAX_IMPORTS simbolos e erro interno. */
bool LigarComTabelas(const LigarPedido *pedido, Imagem *im, Diag *erro);

/* Liga programa (vem antes) e runtime (vem depois) em im, que tem de vir de ImagemIniciar. O programa entra inteiro; do runtime entra o que a
 * forja_entrada alcanca por relocacao (secao a secao, com o .pdata/.xdata do que ficou) e so os __imp_ alcancaveis vao para o .idata.
 * Devolve false com X0903 (simbolo citado por secao viva indefinido ou duplicado, __imp_ citado fora de KERNEL32) ou X0905 (.pdata fora de ordem).
 * Em falha a Imagem fica parcial: quem chamou libera com ImagemLiberar. */
bool Ligar(const Objeto *programa, const Objeto *runtime, Imagem *im, Diag *erro);

#endif
