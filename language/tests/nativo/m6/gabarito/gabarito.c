/* gabarito.c - controle positivo do verificar_gdb.py (M6 P64). Forma igual a das fixtures .fj: 3 niveis de proc, int, double, bool,
 * struct por valor e str (const char *). Compilado so em %TEMP% com gcc -g -gdwarf-4 -O0 -fno-omit-frame-pointer (MinGW e alpine). */
#include <stdbool.h>
#include <stdio.h>

struct Ponto {
    int x;
    double y;
};

static int nivel3(int a, struct Ponto p, const char *texto)
{
    int soma = a + p.x;
    double media = p.y * 2.0;
    bool ok = soma > 0;
    if (ok) {
        printf("%s %d %f\n", texto, soma, media);
    }
    return soma;
}

static int nivel2(int a)
{
    struct Ponto p = {a, 1.5};
    return nivel3(a + 1, p, "marca");
}

static int nivel1(int a)
{
    return nivel2(a + 1);
}

int main(void)
{
    return nivel1(3) == 9 ? 0 : 1;
}
