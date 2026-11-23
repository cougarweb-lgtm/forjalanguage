/* gnuimp.c - DLL de C de teste da bateria de interop (P19i-a, caso c_import_gnu): a Forja liga pela import library GNU (libgnuimp.dll.a, dlltool).
 * A DLL (dll\gnuimp.dll) fica FORA da pasta de busca da ligacao: so a .dll.a esta la; a DLL so aparece no PATH do run (caminho.txt).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19i-a: primeira versao.
 * Verificacao: python -I construir_lib.py e python -I ..\..\..\verificar_interop.py --forja <copia> --backend c --pasta dinamica --caso c_import_gnu
 */
#include <stdint.h>

int32_t gi_soma3(int32_t a, int32_t b, int32_t c) { return a + b + c; }

uint64_t gi_fatorial(uint32_t n)
{
    uint64_t r = 1;
    for (uint32_t i = 2; i <= n; i++) r *= i;
    return r;
}

double gi_hipotenusa2(double a, double b) { return a * a + b * b; }

const char *gi_nome(void) { return "importada pela .dll.a"; }
