/* fx_coff_comdat.h - M3b P23e: o que dois membros de libfx_coff.a repetem e o ligador tem de juntar em UM: uma funcao inline e um dado selectany (COMDAT, mais o .pdata/.xdata
 * associado da funcao). Compilado como C++ (g++ -O0 -fno-exceptions) por coff_a.cpp e coff_b.cpp. */
#ifndef FX_COFF_COMDAT_H
#define FX_COFF_COMDAT_H

extern "C" {
inline int fx_soma_inline(int a, int b)
{
    return a * 10 + b;
}

__declspec(selectany) int fx_tabela_sel[4] = {3, 5, 7, 11};
}

#endif
