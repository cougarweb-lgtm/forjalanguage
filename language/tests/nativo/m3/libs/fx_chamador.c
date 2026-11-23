/* fx_chamador.c - DLL de teste dos CALLBACKS do backend nativo x64 (M3 P21): a DLL guarda ou recebe um ponteiro de funcao (o trampolim fjt_ da Forja) e o chama.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P21: primeira versao.
 * Verificacao: construir_libs.py --so-callback e python -I ..\..\verificar_nativo.py --pasta m3/callback
 * Estruturas por valor no Win64: 8 bytes viaja num registrador (bytes do inteiro), 24 bytes viaja por ponteiro para uma COPIA do chamador (o callback pode alterar a copia).
 */
#include <stdint.h>
#include <windows.h>

#define FX __declspec(dllexport)

typedef struct { int32_t a, b; } P8;
typedef struct { int64_t x, y, z; } P24;

/* (i32, f64, ^T, struct de 8 B) -> i32 */
typedef int32_t (*CbMisto)(int32_t, double, int32_t *, P8);
FX int32_t fx_chama_misto(CbMisto cb, int32_t *alvo)
{
    P8 p = {3, 4};

    return cb(7, 2.5, alvo, p);
}

/* (struct de 24 B por ponteiro) -> i64: o callback altera a copia e o original do chamador continua igual */
typedef int64_t (*CbGrande)(P24);
FX int64_t fx_chama_grande(CbGrande cb)
{
    P24 g = {10, 20, 30};
    int64_t r = cb(g);

    return r * 1000 + g.x + g.y + g.z;
}

/* retorno de struct por valor: 8 B no RAX, 24 B por ponteiro oculto */
typedef P8 (*CbRetorna8)(int32_t, int32_t);
typedef P24 (*CbRetorna24)(int64_t);
FX int64_t fx_chama_retorna(CbRetorna8 cb8, CbRetorna24 cb24)
{
    P8 a = cb8(5, 6);
    P24 b = cb24(100);

    return (int64_t)a.a * 1000000 + a.b * 10000 + b.x + b.y + b.z;
}

/* estilo glfw: o callback fica guardado e e chamado depois, por outra funcao (3 argumentos; erro com cstr) */
typedef void (*CbTamanho)(void *, int32_t, int32_t);
typedef void (*CbErro)(int32_t, const char *);
static CbTamanho g_tamanho;
static CbErro g_erro;

FX void fx_registra_tamanho(CbTamanho cb) { g_tamanho = cb; }
FX void fx_dispara_tamanho(void *janela, int32_t w, int32_t h) { g_tamanho(janela, w, h); }
FX void fx_registra_erro(CbErro cb) { g_erro = cb; }
FX void fx_dispara_erro(int32_t codigo, const char *texto) { g_erro(codigo, texto); }

/* user_data: o dado do usuario volta como rawptr (convertido a ^T no callback) ou ja como ^i32 */
typedef void (*CbDado)(void *, int32_t);
typedef void (*CbDadoTipado)(int32_t *, int32_t);
FX void fx_chama_n(CbDado cb, void *dado, int32_t n)
{
    for (int32_t i = 1; i <= n; i++) cb(dado, i);
}
FX void fx_chama_tipado(CbDadoTipado cb, int32_t *dado, int32_t n) { cb(dado, n); }

/* abi f: quantos quadros do RtlCaptureStackBackTrace caem dentro do exe ou desta DLL (o desenrolar de pilha tem de atravessar o fjt_ e os procs da Forja) */
FX int32_t fx_quadros_nos_modulos(void)
{
    void *quadros[64];
    HMODULE esta = NULL;
    HMODULE exe = GetModuleHandleW(NULL);
    int32_t dentro = 0;
    USHORT n = RtlCaptureStackBackTrace(0, 64, quadros, NULL);

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&fx_quadros_nos_modulos, &esta);
    for (USHORT k = 0; k < n; k++) {
        HMODULE dono = NULL;

        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)quadros[k], &dono);
        if (dono != NULL && (dono == exe || dono == esta)) dentro++;
    }
    return dentro;
}

/* A recursao passa pela DLL (Forja -> C -> Forja ...): o gcc do oraculo nao enxerga o outro lado e nao funde os quadros. */
typedef int32_t (*CbNivel)(int32_t);
FX int32_t fx_chama_nivel(CbNivel cb, int32_t n)
{
    int32_t r = cb(n); /* -O0: sem chamada em cauda, o quadro desta funcao fica na pilha */

    return r;
}
