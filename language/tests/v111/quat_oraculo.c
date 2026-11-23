/* quat_oraculo.c - oraculo do L14b (Linguagem v1.11, A05): 200 casos de q * r, q * v e abs(q) calculados pelo raymath.h da raylib 6.2 e impressos em bits.
 *
 * Cada linha (8 palavras f32 em hexa de 8 digitos): QuaternionMultiply(q, r) x y z w, Vector3RotateByQuaternion(v, q) x y z e QuaternionLength(q).
 * A fixture tests\lowering\189_quat_raymath.fj gera os mesmos 200 casos em Forja e imprime as mesmas palavras; o stdout dos dois tem de ser IGUAL byte a byte
 * (tests\v111\conferir_quat_oraculo.py roda os dois e imprime "quat_oraculo N/200").
 *
 * Gerador (documentado; sem ponto flutuante): LCG de 31 bits, estado = (estado * 1103515245 + 12345) % 2147483648, semente 20261109.
 * Cada valor f32 sai do estado novo: n = (estado / 65536) % 4001 e valor = (float)(n - 2000) / 1000.0f, em [-2.000, 2.000] de 0.001 em 0.001.
 * Por caso, 11 sorteios na ordem q.x q.y q.z q.w r.x r.y r.z r.w v.x v.y v.z.
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.11 L14b: primeira versao. Funcoes do raymath.h conferidas no arquivo interop_lab\raylib\vendor\include\raymath.h:
 *            Vector3RotateByQuaternion (linha 906), QuaternionLength (linha 2185), QuaternionMultiply (linha 2230).
 * Verificacao: gcc -O0 -ffp-contract=off -I <pasta do raymath.h> quat_oraculo.c -o quat_oraculo.exe -lm   (ou python conferir_quat_oraculo.py)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RAYMATH_STATIC_INLINE /* raymath.h:26: as funcoes entram como static inline neste arquivo, sem libraylib */
#include "raymath.h"

enum { CASOS = 200, DIVISOR_VALOR = 1000, LIMITE_N = 4001, ZERO_N = 2000 };
#define SEMENTE 20261109ULL
#define LCG_A 1103515245ULL
#define LCG_C 12345ULL
#define LCG_M 2147483648ULL

static uint32_t Bits(float x)
{
    uint32_t b;
    memcpy(&b, &x, sizeof b);
    return b;
}

/* O proximo valor f32 do gerador. */
static float Proximo(uint64_t *estado)
{
    *estado = (*estado * LCG_A + LCG_C) % LCG_M;
    int64_t n = (int64_t)((*estado / 65536ULL) % LIMITE_N);
    return (float)(n - ZERO_N) / (float)DIVISOR_VALOR;
}

int main(void)
{
    uint64_t estado = SEMENTE;
    for (int caso = 0; caso < CASOS; caso++) {
        Quaternion q, r;
        Vector3 v;
        q.x = Proximo(&estado);
        q.y = Proximo(&estado);
        q.z = Proximo(&estado);
        q.w = Proximo(&estado);
        r.x = Proximo(&estado);
        r.y = Proximo(&estado);
        r.z = Proximo(&estado);
        r.w = Proximo(&estado);
        v.x = Proximo(&estado);
        v.y = Proximo(&estado);
        v.z = Proximo(&estado);
        Quaternion p = QuaternionMultiply(q, r);
        Vector3 g = Vector3RotateByQuaternion(v, q);
        float norma = QuaternionLength(q);
        printf("%08x %08x %08x %08x %08x %08x %08x %08x\n", (unsigned)Bits(p.x), (unsigned)Bits(p.y), (unsigned)Bits(p.z), (unsigned)Bits(p.w),
               (unsigned)Bits(g.x), (unsigned)Bits(g.y), (unsigned)Bits(g.z), (unsigned)Bits(norma));
    }
    return 0;
}
