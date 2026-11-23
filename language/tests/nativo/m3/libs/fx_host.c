/* fx_host.c - executavel que EXPORTA uma API e carrega uma DLL da Forja que a importa (M3 P22b; exe_host; usa o P19b: exe como fonte de simbolo).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22b: primeira versao.
 * Uso: fx_host.exe <dll>   (carrega a DLL, chama exe_host_roda(10) e imprime o que a DLL devolveu e o contador do host)
 * Build: gcc -O1 -o fx_host.exe fx_host.c -Wl,--out-implib,libfx_host.dll.a  (a import library nomeia o MODULO "fx_host.exe": o loader acha o proprio exe ja carregado)
 * A DLL da Forja liga com 'foreign { link "fx_host" ... }': o gcc usa libfx_host.dll.a e o backend x64 le a mesma import library (BIB_IMPORT_GNU).
 * Verificacao: python -I ..\..\abi\verificar_abi.py --lado export --forja <copia>
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define ARGUMENTO_DA_RODADA 10

static int64_t g_contador;

__declspec(dllexport) int64_t fx_host_soma(int64_t a, int64_t b)
{
    return a + b;
}

__declspec(dllexport) int64_t fx_host_contar(void)
{
    return ++g_contador;
}

__declspec(dllexport) double fx_host_media(double a, double b)
{
    return (a + b) / 2.0;
}

__declspec(dllexport) void fx_host_diz(const char *texto)
{
    printf("host diz: %s\n", texto);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    HMODULE m;
    int64_t (*roda)(int64_t);
    int64_t resultado;

    if (argc < 2) {
        fprintf(stderr, "uso: fx_host.exe <dll>\n");
        return 2;
    }
    m = LoadLibraryA(argv[1]);
    if (m == NULL) {
        printf("fx_host: LoadLibrary(%s) falhou: %lu\n", argv[1], GetLastError());
        return 3;
    }
    roda = (int64_t (*)(int64_t))GetProcAddress(m, "exe_host_roda");
    if (roda == NULL) {
        printf("fx_host: a DLL nao exporta exe_host_roda\n");
        return 4;
    }
    resultado = roda(ARGUMENTO_DA_RODADA);
    printf("fx_host: exe_host_roda(%d) = %lld; contador do host = %lld\n", ARGUMENTO_DA_RODADA, (long long)resultado, (long long)g_contador);
    fflush(stdout);
    return 0;
}
