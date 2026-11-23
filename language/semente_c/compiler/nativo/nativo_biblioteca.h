/* nativo_biblioteca.h - cola do backend nativo com bibliotecas.h: pega os 'link' e os simbolos de foreign do programa e entrega as tabelas de imports ao ligador (P19 do M3).
 * Versao: 0.7.0
 * Changelog: 0.7.0 - 0.21.0 F4 passo 2b: sai o prototipo de NativoConferirBibliotecasDoC (backend C aposentado).
 *            0.6.0 - M6d P92c: NativoBibliotecasEntrada.libmem.
 *            0.5.0 - M5c-b P57: NativoUsaThreadsNoElf.
 *            0.4.0 - IA1-b03: NativoConferirBibliotecasDoC.
 *            0.3.0 - M3b P23f: NativoBibliotecasEntrada (NativoBibliotecas recebe pedido, BIR e provedores numa struct).
 *            0.2.0 - M3 P22-O3: NativoUsaRuntimeCrt.
 *            0.1.0 - M3 P19: NativoBibliotecas (acha os simbolos de BS_IMPORT na BIR, resolve contra os 'link' e, na falha, monta o diagnostico X01xx com a posicao da
 *                    declaracao do foreign e os campos proc, simbolo, biblioteca e procurei_em).
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m3/foreign e python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#ifndef FORJA_NATIVO_NATIVO_BIBLIOTECA_H
#define FORJA_NATIVO_NATIVO_BIBLIOTECA_H

#include "nativo/nativo.h"
#include "nativo/obj/bibliotecas.h"

/* O diagnostico de uma biblioteca que falhou: o Diag (codigo, causa e off = a declaracao do foreign) e os campos extras do JSON ("proc": ..., sem a virgula
 * inicial; ErroOrigem.campos). Chame BibliotecaFalhaLiberar sempre. */
typedef struct {
    Diag diag;
    Buf campos;
} BibliotecaFalha;

/* O que NativoBibliotecas le: o pedido, a BIR e os provedores (BibliotecasProvedor: os objetos da ligacao que satisfazem simbolos pedidos por membros de estatica; podem ser zero). */
typedef struct {
    const NativoPedido *pedido;
    const BirPrograma *bir;
    const BibliotecasProvedor *provedores;
    uint32_t qtd_provedores;
    bool libmem;  /* M6d P92c: dev x64 (nao DLL): NativoBibliotecas monta o mapa slot -> lib (BibliotecasResolvidas.libmem); false = zerado */
} NativoBibliotecasEntrada;

/* Resolve os 'link' do programa contra os simbolos de foreign que a BIR chama (BS_IMPORT). Sem 'link' e sem foreign de fora devolve true com 0 tabelas (nada a ligar).
 * Devolve false em X0101 (simbolo), X0102 (link sem arquivo), X0103 (formato ainda nao suportado) ou X0105 (PE de 32 bits, malformado ou objeto /GL); *falha diz qual.
 * Chame BibliotecasLiberar(saida) depois, ate quando devolver false. */
bool NativoBibliotecas(const NativoBibliotecasEntrada *entrada, BibliotecasResolvidas *saida, BibliotecaFalha *falha);

/* O produto usa a variante CRT do runtime (rt_saida.c, FORJA_RT_CRT)? Verdadeiro para toda DLL e para programa com foreign de fora (BS_IMPORT). */
bool NativoUsaRuntimeCrt(const NativoPedido *pedido, const BirPrograma *bir);

/* M5c-b P57: o programa referencia forja_thread_* (std:thread/std:async)? No alvo linux isso liga a variante libc do runtime (musl), como um foreign. */
bool NativoUsaThreadsNoElf(const BirPrograma *bir);

void BibliotecaFalhaLiberar(BibliotecaFalha *falha);

#endif
