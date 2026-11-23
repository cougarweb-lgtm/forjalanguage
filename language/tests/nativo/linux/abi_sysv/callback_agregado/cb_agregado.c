/* cb_agregado.c - lib da fixture de callback com agregado SysV (M5b P38c), build no alpine por construir_callback_agregado.sh.
 * cba_chama24 (24 B, MEMORY nos dois sentidos), cba_chama12 (12 B SSE+SSE), cba_chama16 (16 B SSE+INTEGER), cba_chama8 (8 B INTEGER):
 * o C chama o valor de proc que a Forja passou (o stub fjt_ SysV) com o agregado por valor e recebe o retorno; depois mexe no retorno.
 * cba_reentra: chama o callback (que, na Forja, chama de novo o C com agregado): reentrancia.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P38c: primeira versao.
 */
typedef struct { long a; double b; long c; } CbaS24;
typedef struct { float x, y, z; } CbaS12;
typedef struct { double d; long l; } CbaS16;
typedef struct { int i; float f; } CbaS8;

CbaS24 cba_chama24(CbaS24 (*cb)(CbaS24, long), CbaS24 s, long k)
{
    CbaS24 r = cb(s, k);

    r.a += 1000;
    return r;
}

CbaS12 cba_chama12(CbaS12 (*cb)(CbaS12), CbaS12 s)
{
    CbaS12 r = cb(s);

    r.z = r.z * 2.0f;
    return r;
}

CbaS16 cba_chama16(CbaS16 (*cb)(CbaS16, long), CbaS16 s, long k)
{
    CbaS16 r = cb(s, k);

    r.l += k;
    return r;
}

CbaS8 cba_chama8(CbaS8 (*cb)(CbaS8, long), CbaS8 s, long t)
{
    return cb(s, t);
}

long cba_reentra(long (*cb)(long), long n)
{
    return cb(n) + 1;
}
