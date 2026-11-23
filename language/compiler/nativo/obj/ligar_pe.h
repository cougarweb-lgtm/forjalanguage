/* ligar_pe.h - o que so o ligador do PE faz depois de aplicar as relocacoes (extraido do ligar.c no M5 P27a, sem mudar comportamento).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5 P27a: LigarPeFinalizar (ordena os DIR64 do .reloc e confere a ordem do .pdata), antes LigarOrdenarDir64 e LigarPdataOrdenado do ligar.c 0.8.3.
 * Verificacao: construir.bat && teste_nativo.exe obj <pasta> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#ifndef FORJA_NATIVO_OBJ_LIGAR_PE_H
#define FORJA_NATIVO_OBJ_LIGAR_PE_H

#include "common/diag.h"
#include "nativo/fmt/pe.h"

/* Ordena em im->dir64 as RVAs dos campos ADDR64 (o .reloc sai por pagina) e confere que as entradas RUNTIME_FUNCTION da secao im->secoes[idx_pdata] vem com BeginAddress
 * crescente (idx_pdata < 0 = o exe nao tem .pdata). X0905 na entrada fora de ordem; false so nesse caso. */
bool LigarPeFinalizar(Imagem *im, int idx_pdata, Diag *erro);

#endif
