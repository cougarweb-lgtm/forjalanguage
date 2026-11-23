/* dwarf.h - DWARF 4 do programa (M6 P66-P67, D-M6-3): .debug_abbrev, .debug_info, .debug_line e .debug_frame como secoes de debug do Objeto
 * (OS_DEBUG_*), montadas a partir do DbgPrograma. Enderecos saem de relocacoes ADDR64 a simbolos do .text: o ligador os resolve (D-M6-4).
 * Os tipos e as variaveis (P67) saem da tabela de tipos e do layout do programa, passados em DwarfSemantica.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6 P68: DwarfSemantica.com_frame (so o ELF leva .debug_frame: a CFI; o PE usa .pdata); DwarfEscrever so cria a secao quando ela vale.
 *            0.2.0 - M6 P67: DwarfSemantica (fonte, tipos, IR e nomes) e a assinatura nova de DwarfEscrever; DIE de tipo e de variavel em dwarf_tipos.c.
 *            0.1.0 - M6 P66: primeira versao (1 CU DW_LANG_C99; subprogramas com low/high_pc e frame_base rbp; linha v4; CFI dos procs).
 * Verificacao: python compiler\construir_rapido.py && python ..\tests\nativo\m6\verificar_gdb.py --forja .\forja.exe --alvo linux
 */
#ifndef FORJA_NATIVO_DBG_DWARF_H
#define FORJA_NATIVO_DBG_DWARF_H

#include "common/fonte.h"
#include "common/intern.h"
#include "ir/ir.h"
#include "nativo/dbg/dbg.h"
#include "nativo/obj/objeto.h"
#include "semantic/tipos.h"

/* O que o DWARF precisa do programa alem do registro: a fonte (arquivos), a tabela de tipos, a IR (structs e enums) e os nomes internados. */
typedef struct {
    const Fonte *fonte;
    const Tipos *tipos;
    const IrPrograma *ir;
    const Interner *nomes;
    bool com_frame;   /* .debug_frame (CFI dos procs): so no ELF (D-M6-3); no PE o gdb usa .pdata */
} DwarfSemantica;

/* Acrescenta as secoes de debug ao objeto do programa. Sem procs, nao acrescenta nada. ELF e PE (P68): o PE nao leva .debug_frame. */
void DwarfEscrever(Objeto *o, const DbgPrograma *d, const DwarfSemantica *s);

#endif
