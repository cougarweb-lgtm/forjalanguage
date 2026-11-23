/* oraculo_agregado.c - oraculo C da fixture callback_agregado (M5b P38c): mesmos callbacks em C e a mesma sequencia de chamadas do .fj.
 * Saida = callback_agregado.esperado. Compilado no alpine-build (verificar_callback_agregado.py); o C e a referencia, nao a Forja.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P38c: primeira versao.
 */
#include <stdio.h>
#include "cb_agregado.c"

static CbaS24 cb24(CbaS24 s, long k)
{
    CbaS24 r = {s.a + k, s.b * 2.0, s.c - k};

    return r;
}

static CbaS12 cb12(CbaS12 v)
{
    CbaS12 r = {v.x + 1.0f, v.y * 2.0f, v.z - 0.5f};

    return r;
}

static CbaS16 cb16(CbaS16 s, long k)
{
    CbaS16 r = {s.d + 0.25, s.l * k};

    return r;
}

static CbaS8 cb8(CbaS8 s, long t)
{
    CbaS8 r = {s.i + (int)t, s.f * 3.0f};

    return r;
}

static long cb_nivel(long n)
{
    CbaS24 r = cba_chama24(cb24, (CbaS24){n, 1.5, 2}, n);

    return r.a;
}

int main(void)
{
    CbaS24 r24 = cba_chama24(cb24, (CbaS24){1, 2.5, 3}, 4);
    CbaS12 r12 = cba_chama12(cb12, (CbaS12){1.5f, 2.0f, 4.0f});
    CbaS16 r16 = cba_chama16(cb16, (CbaS16){1.5, 7}, 3);
    CbaS8 r8 = cba_chama8(cb8, (CbaS8){10, 1.5f}, 5);

    printf("%ld\n", r24.a);
    printf("%ld\n", (long)(r24.b * 10.0));
    printf("%ld\n", r24.c);
    printf("%ld\n", (long)(r12.x * 10.0f));
    printf("%ld\n", (long)(r12.y * 10.0f));
    printf("%ld\n", (long)(r12.z * 10.0f));
    printf("%ld\n", (long)(r16.d * 100.0));
    printf("%ld\n", r16.l);
    printf("%ld\n", (long)r8.i);
    printf("%ld\n", (long)(r8.f * 10.0f));
    printf("%ld\n", cba_reentra(cb_nivel, 6));
    return 0;
}
