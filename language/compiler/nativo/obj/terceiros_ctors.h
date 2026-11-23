/* terceiros_ctors.h - tabela de construtores do 4o objeto do ligador: o que as secoes .CRT$XC* (MSVC) e .ctors / .ctors.NNNNN (gcc) dos membros de bibliotecas estaticas registram (P23f do M3b).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3b P23f: TerceirosConstrutores.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_nativo.py --pasta m3/estatica
 */
#ifndef FORJA_NATIVO_OBJ_TERCEIROS_CTORS_H
#define FORJA_NATIVO_OBJ_TERCEIROS_CTORS_H

#include "nativo/obj/objeto.h"

/* Os dois simbolos que o runtime (rt_legado.c, forja_entrada_ctor) le: o inicio e o fim da tabela de ponteiros de funcao, na ordem em que rodam. */
#define TERCEIROS_SIMBOLO_CTORS_INI "forja_ctors_ini"
#define TERCEIROS_SIMBOLO_CTORS_FIM "forja_ctors_fim"

/* Junta os ponteiros das secoes de construtores de o (ja ordenado por terceiros.c) numa secao .rdata$ctors, com os dois simbolos acima, na ordem de execucao: primeiro os .CRT$XC*
 * (pelo nome da secao, como o _initterm do CRT) e depois os .ctors do gcc na ordem INVERSA da lista do ld (o ld poe o .ctors sem sufixo antes dos .ctors.NNNNN em ordem crescente
 * e o __do_global_ctors roda do fim para o comeco: .ctors.65434, depois .ctors.65433, por ultimo o .ctors sem sufixo; dentro de cada secao, do ultimo ponteiro ao primeiro).
 * Devolve true se criou a tabela (havia algum construtor). As secoes originais ficam, sem ninguem que as cite: o ligador as descarta. */
bool TerceirosConstrutores(Objeto *o);

#endif
