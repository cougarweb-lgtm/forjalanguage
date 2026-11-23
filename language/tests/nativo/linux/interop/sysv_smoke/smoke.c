/* smoke.c - prova da ABI SysV no call site do foreign (M5b P37): struct por valor (INTEGER+SSE, 12 B, 24 B = MEMORY), real f32/f64 e retorno.
 * Compilada no WSL (alpine-build) por construir.sh: vendor/linux/libsmoke.a.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P37: primeira versao.
 */
typedef struct { int a; double b; } PAB;
typedef struct { int a; int b; int c; } P12;
typedef struct { double x, y, z; } P24;

/* Sem literal real no corpo: o ligador ELF do P37 ainda nao mapeia .rodata de membro (achado); as contas usam so soma. */
PAB sv_mistura(PAB p)
{
    PAB r = {p.a + p.a, p.b + p.b + p.b};
    return r;
}

double sv_f(float a, double b, int c)
{
    return a + b + c;
}

P24 sv_grande(P24 p)
{
    p.x = p.x + p.y;
    p.y = p.y + p.z;
    p.z = p.z + p.x;
    return p;
}

int sv_soma12(P12 p)
{
    return p.a + p.b + p.c;
}

P12 sv_dobra12(P12 p)
{
    P12 r = {p.a + p.a, p.b + p.b, p.c + p.c};
    return r;
}
