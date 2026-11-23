/* nativo_interface.h - nativo_interface.c: o forja_dll_info de toda DLL Forja (M8 H1, D-M8-2).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - G3 G0b: quebra de ciclo bir->nativo (DLL_INFO_NOME e DLL_INFO_SIMBOLO_DADOS foram para bir/bir.h).
 *            0.1.1 - M8 P2: NativoAbrirRdata (publica).
 *            0.1.0 - M8 H1: primeira versao.
 * Verificacao: python -I ..\tests\rodar_bateria.py --so interop (caso dinamica\dll_info)
 */
#ifndef FORJA_NATIVO_NATIVO_INTERFACE_H
#define FORJA_NATIVO_NATIVO_INTERFACE_H

#include "nativo/nativo.h"
#include "nativo/obj/objeto.h"

/* DLL_INFO_SIMBOLO_DADOS (a struct estatica que a BirFuncao forja_dll_info devolve) e DLL_INFO_NOME ficam em bir/bir.h (G3 G0b: quebra de ciclo bir->nativo). */

/* Acrescenta ao .rdata do programa (build --modo dll) a struct de 32 bytes com o texto canonico da interface e o hash FNV-1a 64 dele, e define DLL_INFO_SIMBOLO_DADOS.
 * O campo texto leva uma relocacao ADDR64 (a DLL ganha uma entrada no .reloc). Sem pedido->dll nao faz nada. Devolve sempre true (so para encadear com o gerar). */
bool NativoInterfaceDll(Objeto *programa, const NativoPedido *pedido);

/* O indice do .rdata do programa (aberto se nao tem nenhum), com o alinhamento subido a 8. */
uint32_t NativoAbrirRdata(Objeto *o);

#endif
