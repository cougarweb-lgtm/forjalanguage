#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.141592653589793

typedef enum { CIRCULO, RETANGULO, QUADRADO } Tipo;

/* Uniao etiquetada: o campo tipo diz qual membro vale. */
typedef struct {
    Tipo tipo;
    union {
        struct { long long raio; } circulo;
        struct { long long largura, altura; } retangulo;
        struct { long long lado; } quadrado;
    } v;
} Forma;

static double area(const Forma *f) {
    switch (f->tipo) {
        case CIRCULO: return PI * (double)f->v.circulo.raio * (double)f->v.circulo.raio;
        case RETANGULO: return (double)(f->v.retangulo.largura * f->v.retangulo.altura);
        case QUADRADO: return (double)(f->v.quadrado.lado * f->v.quadrado.lado);
    }
    return 0.0;
}

int main(int argc, char **argv) {
    int i = 1;
    while (i < argc) {
        Forma f;
        const char *nome = argv[i];
        if (strcmp(nome, "circulo") == 0 && i + 1 < argc) {
            f.tipo = CIRCULO;
            f.v.circulo.raio = strtoll(argv[i + 1], NULL, 10);
            i += 2;
        } else if (strcmp(nome, "retangulo") == 0 && i + 2 < argc) {
            f.tipo = RETANGULO;
            f.v.retangulo.largura = strtoll(argv[i + 1], NULL, 10);
            f.v.retangulo.altura = strtoll(argv[i + 2], NULL, 10);
            i += 3;
        } else if (strcmp(nome, "quadrado") == 0 && i + 1 < argc) {
            f.tipo = QUADRADO;
            f.v.quadrado.lado = strtoll(argv[i + 1], NULL, 10);
            i += 2;
        } else {
            printf("forma desconhecida: %s\n", nome);
            return 1;
        }
        printf("%s: %.2f\n", nome, area(&f));
    }
    return 0;
}
