/* carga_pacote.h - carga do modulo de pasta (Linguagem v1.13): 'import "dir"' junta os .fj da pasta num pacote (um modulo, varios trechos da Fonte).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.13 L1a: CargaPacote, CargaJaEntrou, CargaRelatarRepetido (E1008).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#ifndef FORJA_DRIVER_CARGA_PACOTE_H
#define FORJA_DRIVER_CARGA_PACOTE_H

#include "driver/compilacao.h"

typedef enum { CARGA_SEGUE_ARQUIVO, CARGA_PACOTE_CARREGADO, CARGA_FALHA } CargaResultado;

/* O modulo k (de arquivo, vendor: ou engine:) e uma pasta? 'caminho_fj' = pasta + resto + ".fj" do import; a pasta e o mesmo caminho sem o ".fj".
 * Pasta com .fj e sem o arquivo irmao: lista os .fj (ordem de byte do nome, sem subpasta), anexa e analisa cada um (AnalisarModulo) com modulo k = CARGA_PACOTE_CARREGADO.
 * Pasta sem .fj junto de um .fj irmao (a pasta de uma biblioteca C): o arquivo vale, CARGA_SEGUE_ARQUIVO. Pasta vazia de .fj sem arquivo irmao = E1001; pasta com .fj e arquivo irmao = E1008.
 * Nao e pasta: CARGA_SEGUE_ARQUIVO. CARGA_FALHA = diagnostico em c->diag. */
CargaResultado CargaPacote(Compilacao *c, uint32_t k, const char *caminho_fj);

/* O arquivo (caminho absoluto igual, sem diferenciar caixa) ja e um trecho da Fonte? */
bool CargaJaEntrou(const Compilacao *c, const char *caminho);

/* E1008 no import do modulo: o arquivo entraria duas vezes. Devolve false. */
bool CargaRelatarRepetido(Compilacao *c, const Modulo *mod, const char *caminho);

#endif
