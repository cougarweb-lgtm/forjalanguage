/* real_curto.h - FormatarRealCurto: texto de um real pelos MENOS digitos que voltam ao mesmo valor (a regra do print de real da Forja).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - G3 G0c: primeira versao. Substitui o "%.9g"/"%.17g" dos dumps de BIR (o texto de real passa a ser o mesmo do runtime: rt_real_curto.c).
 * Verificacao: construir.bat && python -I ..\tests\rodar_bateria.py --so bir (dumps de BIR com real)
 */
#ifndef FORJA_REAL_CURTO_H
#define FORJA_REAL_CURTO_H

#include "common/buf.h"

/* Anexa em 'saida' o texto de 'valor': menor numero de digitos que reconstroi o mesmo valor (f32 = true: o mesmo float; senao o mesmo double), sempre posicional
 * e sem ".0" sobrando ("1", "0.5", "0.0001", "10000000000"); "nan", "inf", "-inf"; zero negativo "-0". Para f32, 'valor' e o float alargado a double. */
void FormatarRealCurto(Buf *saida, double valor, bool f32);

#endif
