/* fx_tipos.c - DLL de teste do foreign do backend nativo x64 (M3 P23): handle opaco, enum com valor (inclusive negativo) e struct por valor com struct
 * aninhada e [N]T em campo. Cada funcao confere o que recebeu na ABI Win64 e devolve um numero que denuncia a leitura errada.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P23: primeira versao.
 * Verificacao: python -I construir_libs.py --so-tipos && python -I ..\..\verificar_nativo.py --pasta m3/foreign
 */
#include <stdint.h>
#include <stdlib.h>

#define FX __declspec(dllexport)

/* opaque: o C so devolve e recebe o ponteiro; a Forja nunca ve o corpo. */
typedef struct FxObj { int32_t valor; int32_t chamadas; } FxObj;

FX FxObj *fx_obj_novo(int32_t valor)
{
    FxObj *o = (FxObj *)malloc(sizeof(FxObj));
    if (o == NULL) return NULL;
    o->valor = valor;
    o->chamadas = 0;
    return o;
}

FX int32_t fx_obj_soma(FxObj *o, int32_t n)
{
    if (o == NULL) return -999;
    o->chamadas += 1;
    o->valor += n;
    return o->valor * 100 + o->chamadas;
}

FX void fx_obj_libera(FxObj *o) { free(o); }

/* enum com valor: Cor { vermelho = 1, verde = 2, azul = 4 } e Nivel { baixo = -1, medio, alto } chegam como int de 32 bits. */
FX int32_t fx_cor_peso(int32_t cor) { return cor * 10; }

FX int32_t fx_cor_proxima(int32_t cor) { return cor == 1 ? 2 : (cor == 2 ? 4 : 1); }

FX int32_t fx_nivel_dobro(int32_t nivel) { return nivel * 2; }

FX int32_t fx_nivel_de(int32_t n) { return n < 0 ? -1 : (n == 0 ? 0 : 1); }

/* struct aninhada por valor. Vec2 (8 B) viaja num registrador; Caixa (16 B), Lote (16 B) e Retangulo (32 B) por ponteiro, retorno pelo ponteiro oculto. */
typedef struct Vec2 { int32_t x; int32_t y; } Vec2;
typedef struct Caixa { Vec2 a; Vec2 b; } Caixa;
typedef struct Lote { int32_t ids[3]; float peso; } Lote;
typedef struct Retangulo { Vec2 pos; Vec2 tam; Vec2 cantos[2]; } Retangulo;

FX Vec2 fx_vec_soma(Vec2 a, Vec2 b)
{
    Vec2 r = { a.x + b.x, a.y + b.y };
    return r;
}

FX Caixa fx_caixa_cresce(Caixa c, int32_t d)
{
    Caixa r = { { c.a.x - d, c.a.y - d }, { c.b.x + d, c.b.y + d } };
    return r;
}

FX int32_t fx_lote_soma(Lote l) { return l.ids[0] + l.ids[1] * 10 + l.ids[2] * 100 + (int32_t)l.peso; }

FX Lote fx_lote_dobra(Lote l)
{
    Lote r = { { l.ids[0] * 2, l.ids[1] * 2, l.ids[2] * 2 }, l.peso * 2.0f };
    return r;
}

FX int32_t fx_ret_soma(Retangulo r)
{
    return r.pos.x + r.pos.y * 10 + r.tam.x * 100 + r.tam.y * 1000 + r.cantos[0].x * 10000 + r.cantos[1].y * 100000;
}
