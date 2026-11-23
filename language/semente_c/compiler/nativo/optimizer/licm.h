/* licm.h - passe `licm` (R6a fatia 1 do SIMD): instrucao PURA de aritmetica invariante do laco sobe para o pre-cabecalho (loop-invariant code motion, sem cargas, sem GRAVA, sem alias).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - R6a-3 (D0704): CONST inteira e conta inteira de definicao unica so de imediatos/CONST nao sobem, e so sobe inteiro enquanto couber no orcamento de GPR do laco (pressao de registradores; ver licm.c 0.4.0). 0.2.0 - R6a-2: alem do que sobe pelas condicoes abaixo, o imediato real de um CMP real no laco vira CONST vreg no pre-cabecalho (ate 4 por laco, em qualquer bloco nao frio).
 *            0.1.0 - R6a: primeira versao (so blocos que rodam em toda volta; pre-cabecalho com SALTA ou DESVIA). Entra depois da `numeracao` e antes do `layout`: a numeracao ja propagou constante e copia e juntou o que repete, e o layout so renumera os blocos (o laco que ele rotaciona ja tem o pre-cabecalho feito).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe> && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe licm
 *
 * Por laco natural (do mais interno para o mais externo) com pre-cabecalho unico (unico predecessor do cabecalho fora do laco, terminando em SALTA para ele; sem pre-cabecalho o laco e recusado, nenhum
 * bloco e criado): sobe a instrucao que (a) e pura e nao trapa: CONST COPIA ADD SUB MUL AND OR XOR NEG NOT EXT_S EXT_Z TRUNC END_SLOT END_SIMBOLO (real: CONST COPIA ADD SUB MUL NEG), nunca DIV REM SHL SHR
 * *_OV CMP SELECIONA nem memoria ou chamada; (b) so le imediato, simbolo ou vreg sem definicao no laco (ou ja subido); (c) escreve um vreg que so ela define no laco, que nao e vivo na entrada do cabecalho (o laco nao le o valor antigo)
 * e que nao e vivo em nenhuma saida do laco (o valor antigo nao sobrevive; por isso a instrucao pode ser especulada com o laco rodando 0 vezes); e (d) esta em bloco que domina todo bloco que volta ao
 * cabecalho (roda em toda volta; ramo raro fica de fora para nao ocupar registrador o laco inteiro). O pre-cabecalho pode terminar em DESVIA (laco com guarda): o que sobe entra antes do CMP + DESVIA e
 * tambem roda no outro ramo (sem trap), que por isso nao pode ter o vreg vivo. Nunca roda com --simbolos.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_LICM_H
#define FORJA_NATIVO_OPTIMIZER_LICM_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

/* Passe da tabela (passes.c): roda no programa inteiro, so release. O relato ganha `licm <funcao>: N instrucoes subiram` por funcao em que algo subiu. */
void OtimLicmAplicar(BirPrograma *programa, Buf *relato);

#endif
