#include <stdio.h>
#include <string.h>

typedef enum { VERDE, AMARELO, VERMELHO, PISCANTE } Estado;

static const char *NOMES[] = {"VERDE", "AMARELO", "VERMELHO", "PISCANTE"};

/* Devolve 1 e grava o novo estado; 0 se o evento e invalido. */
static int transicao(Estado atual, const char *evento, Estado *novo) {
    if (strcmp(evento, "tick") == 0) {
        switch (atual) {
            case VERDE: *novo = AMARELO; break;
            case AMARELO: *novo = VERMELHO; break;
            case VERMELHO: *novo = VERDE; break;
            case PISCANTE: *novo = PISCANTE; break;
        }
        return 1;
    }
    if (strcmp(evento, "pane") == 0) {
        *novo = PISCANTE;
        return 1;
    }
    if (strcmp(evento, "ok") == 0) {
        *novo = (atual == PISCANTE) ? VERMELHO : atual;
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    Estado estado = VERDE;
    printf("inicio: VERDE\n");
    for (int i = 1; i < argc; i++) {
        Estado novo = estado;
        if (transicao(estado, argv[i], &novo)) {
            estado = novo;
            printf("%s: %s\n", argv[i], NOMES[estado]);
        } else {
            printf("%s: evento invalido\n", argv[i]);
        }
    }
    return 0;
}
