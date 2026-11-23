/* gabarito_dormir.c - alvo do minidump do verificar_pdb.py (M6b P74). Mesma forma do gabarito.c de M6 (3 niveis de proc,
 * int, double, bool, struct por valor, str), mas nivel3 dorme ms milissegundos depois de imprimir, para o verificador
 * congelar a thread dentro do Sleep. Uso: gabarito_dormir.exe [ms] (padrao 60000). Compilado so em %TEMP% com
 * gcc -g -gdwarf-4 -O0 -fno-omit-frame-pointer. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

struct Ponto {
    int x;
    double y;
};

static int nivel3(int a, struct Ponto p, const char *texto, int ms)
{
    int soma = a + p.x;
    double media = p.y * 2.0;
    bool ok = soma > 0;
    if (ok) {
        printf("%s %d %f\n", texto, soma, media);
    }
    fflush(stdout);
    Sleep((DWORD)ms);
    return soma;
}

static int nivel2(int a, int ms)
{
    struct Ponto p = {a, 1.5};
    return nivel3(a + 1, p, "marca", ms);
}

static int nivel1(int a, int ms)
{
    return nivel2(a + 1, ms);
}

int main(int argc, char **argv)
{
    int ms = argc > 1 ? atoi(argv[1]) : 60000;
    return nivel1(3, ms) == 9 ? 0 : 1;
}
