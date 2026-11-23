/* fadec_envelope.c - envelope do encoder do fadec: neutraliza __builtin_expect para o FE_JMPL sobreviver no Windows (LLP64).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - backend x64 P4b: __builtin_expect(x, y) vira (x). No Windows o builtin recebe long de 32 bits e perde o bit
 *                    FE_JMPL (0x100000000), por isso 35 de 2098 casos do encode-test do fadec falhavam (decisao ENGINE_GAME3D-D0497).
 *                    O fonte de terceiros fica byte a byte; so o envelope muda a macro.
 * Verificacao: construir.bat (compila este arquivo, nunca third\fadec\encode.c direto) e teste_nativo.exe (OK: N verificacoes)
 */
#define __builtin_expect(x, y) (x)
#include "../../../third/fadec/encode.c"
