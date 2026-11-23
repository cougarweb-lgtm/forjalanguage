/*
 * rt_math.c - libm do musl para o runtime nativo da Forja (S3 P2, L11a-2): forja_math_sin, forja_math_pow...
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - L11a-2: primeira versao. Inclui runtime/forja_rt_math.h com FORJA_RT_MATH_EXPORTAR: as 44 funcoes publicas saem globais (forja_math_*); o resto fica static.
 *           O mesmo header vai ao C emitido pelo backend (emit_c_runtime.c), onde sai static. Objeto rt_math.o: ligado so quando o programa referencia forja_math_* (nativo.c LerMath).
 *           Compilado com %FLAGS_RT% -ffp-contract=off -fno-math-errno (construir.bat). Decisao D4: a mesma fonte nos 2 backends = bits iguais.
 *
 * Verificacao: python -I tests\nativo\s3\math\verificar_math.py
 */
#define FORJA_RT_MATH_EXPORTAR
#include "../forja_rt_math.h"
