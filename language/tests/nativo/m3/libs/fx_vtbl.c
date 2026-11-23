/* fx_vtbl.c - DLL de teste da CHAMADA INDIRETA por tabela de funcoes no estilo COM (M3 P21v-b): objeto com lpVtbl, AddRef/Release, 6 argumentos (2 na pilha), f32, f64 e retorno por __ret.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P21v-b: primeira versao.
 * Verificacao: construir_libs.py --so-vtbl e python -I ..\..\verificar_nativo.py --pasta m3/proc_valor
 * A Forja declara a tabela como struct de campos proc e chama q.lpVtbl.Release(q). Win64: o 1o argumento e o objeto (RCX); retorno de struct de 24 B volta por ponteiro oculto (o 2o argumento, __ret).
 */
#include <stdint.h>

#define FX __declspec(dllexport)

typedef struct Obj Obj;
typedef struct { int32_t id; int64_t grande; double fator; } Desc;

typedef struct {
    int32_t (*AddRef)(Obj *);
    int32_t (*Release)(Obj *);
    int32_t (*Soma6)(Obj *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
    float (*Escala)(Obj *, float);
    double (*Media)(Obj *, double, double);
    Desc *(*GetDesc)(Obj *, Desc *);
} Vtbl;

struct Obj {
    const Vtbl *lpVtbl;
    int32_t refs;
    int32_t id;
};

static Obj objetos[4];
static int32_t vivos;

static int32_t AddRefImpl(Obj *o)
{
    return ++o->refs;
}

static int32_t ReleaseImpl(Obj *o)
{
    int32_t r = --o->refs;

    if (r == 0) vivos--;
    return r;
}

static int32_t Soma6Impl(Obj *o, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    return o->id + a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f;
}

static float EscalaImpl(Obj *o, float x)
{
    return x * (float)o->id + 0.5f;
}

static double MediaImpl(Obj *o, double a, double b)
{
    return (a + b) / 2.0 + (double)o->id;
}

static Desc *GetDescImpl(Obj *o, Desc *ret)
{
    ret->id = o->id;
    ret->grande = 5000000000LL + o->id;
    ret->fator = 0.25 * o->id;
    return ret;
}

static const Vtbl TABELA = {AddRefImpl, ReleaseImpl, Soma6Impl, EscalaImpl, MediaImpl, GetDescImpl};

/* Cria o objeto de indice k (0..3) com uma referencia; id = 10 + k. */
FX Obj *fx_criar(int32_t k)
{
    Obj *o = &objetos[k & 3];

    o->lpVtbl = &TABELA;
    o->refs = 1;
    o->id = 10 + (k & 3);
    vivos++;
    return o;
}

/* Quantos objetos ainda tem referencia. */
FX int32_t fx_vivos(void)
{
    return vivos;
}

/* A DLL tambem chama pela propria tabela (C -> C): prova que a mesma tabela serve aos dois lados. */
FX int32_t fx_soma_pela_tabela(Obj *o)
{
    return o->lpVtbl->Soma6(o, 1, 1, 1, 1, 1, 1);
}
