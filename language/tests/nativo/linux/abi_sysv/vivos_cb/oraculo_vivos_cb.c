/* oraculo_vivos_cb.c - oraculo C da fixture vivos_cb.fj (M5b P38-0): o mesmo callback e o mesmo main, em C, compilados no alpine
 * com vivos_cb.c. A saida deste programa e o vivos_cb.esperado. Gerado a mao (fixture); nao e codigo do compilador.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P38-0: primeira versao (espelho linha a linha de vivos_cb.fj).
 */
#include <stdio.h>
#include "vivos_cb.c"

static long cbv_c(long a, long b)
{
    long x1 = a + 1, x2 = a + 2, x3 = a + 3, x4 = a + 4, x5 = a + 5, x6 = a + 6;
    long x7 = a + 7, x8 = a + 8, x9 = a + 9, x10 = a + 10, x11 = a + 11, x12 = a + 12;
    double f1 = 0.25, f2 = 0.5, f3 = 0.75, f4 = 1.0, f5 = 1.25, f6 = 1.5;
    double f7 = 1.75, f8 = 2.0, f9 = 2.25, f10 = 2.5, f11 = 2.75, f12 = 3.0;
    long al = vvc_alinhamento();
    long soma_x = x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 + x11 + x12;
    printf("%lld\n", (long long)al);
    printf("%lld\n", (long long)(soma_x + b));
    printf("%lld\n", (long long)((f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12) * 4.0));
    return soma_x + b + al;
}

int main(void)
{
    long mism = 0;
    long i1 = 1, i2 = 2, i3 = 3, i4 = 4, i5 = 5, i6 = 6, i7 = 7, i8 = 8, i9 = 9, i10 = 10, i11 = 11, i12 = 12;
    double f1 = 0.25, f2 = 0.5, f3 = 0.75, f4 = 1.0, f5 = 1.25, f6 = 1.5;
    double f7 = 1.75, f8 = 2.0, f9 = 2.25, f10 = 2.5, f11 = 2.75, f12 = 3.0;
    long r = vvc_chama_cb(cbv_c, 3, 4, &mism);
    printf("%lld\n", (long long)mism);
    printf("%lld\n", (long long)r);
    printf("%lld\n", (long long)(i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10 + i11 + i12));
    printf("%lld\n", (long long)((f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12) * 4.0));
    return 0;
}
