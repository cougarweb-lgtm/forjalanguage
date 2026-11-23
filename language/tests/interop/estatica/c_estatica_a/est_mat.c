/* est_mat.c - membro 1 de libest.a (P19i-b, caso c_estatica_a): inteiros, ponteiro de buffer, struct por valor e por ponteiro, hash.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19i-b: primeira versao.
 * Verificacao: python -I construir_lib.py e python -I ..\..\verificar_interop.py --forja <copia> --backend c --pasta estatica --caso c_estatica_a
 */
#include <stdint.h>

typedef struct { int32_t x; int32_t y; } est_ponto;

/* n! em 64 bits; -1 fora de 0..20 (21! estoura int64). */
int64_t est_fatorial(int32_t n)
{
    if (n < 0 || n > 20) return -1;
    int64_t r = 1;
    for (int32_t i = 2; i <= n; i++) r *= i;
    return r;
}

/* Maximo divisor comum de Euclides; sempre >= 0 (o resto de C leva o sinal do dividendo, por isso o ajuste final). */
int32_t est_mdc(int32_t a, int32_t b)
{
    while (b != 0) {
        int32_t t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

/* Ordena v[0..n) em ordem crescente, no lugar (insercao). */
void est_ordena(int32_t *v, int32_t n)
{
    for (int32_t i = 1; i < n; i++) {
        int32_t chave = v[i];
        int32_t j = i - 1;
        while (j >= 0 && v[j] > chave) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = chave;
    }
}

/* FNV-1a de 32 bits (offset 2166136261, primo 16777619) sobre os bytes do texto. */
uint32_t est_fnv1a(const char *texto)
{
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)texto; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

/* Struct por valor (8 bytes, passa em registrador no Windows x64): distancia ao quadrado em 64 bits. */
int64_t est_distancia2(est_ponto a, est_ponto b)
{
    int64_t dx = (int64_t)a.x - b.x;
    int64_t dy = (int64_t)a.y - b.y;
    return dx * dx + dy * dy;
}

/* Struct por ponteiro: soma (dx, dy) ao ponto. */
void est_ponto_mover(est_ponto *p, int32_t dx, int32_t dy)
{
    p->x += dx;
    p->y += dy;
}
