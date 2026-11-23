#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACIDADE 4

/* Fila circular: vetor fixo de CAPACIDADE posicoes reaproveitado. */
typedef struct {
    long long itens[CAPACIDADE];
    int inicio;
    int tamanho;
} Fila;

static int enfileirar(Fila *fila, long long valor) {
    if (fila->tamanho == CAPACIDADE) {
        return 0;
    }
    fila->itens[(fila->inicio + fila->tamanho) % CAPACIDADE] = valor;
    fila->tamanho++;
    return 1;
}

static int desenfileirar(Fila *fila, long long *valor) {
    if (fila->tamanho == 0) {
        return 0;
    }
    *valor = fila->itens[fila->inicio];
    fila->inicio = (fila->inicio + 1) % CAPACIDADE;
    fila->tamanho--;
    return 1;
}

int main(int argc, char **argv) {
    Fila fila = {{0}, 0, 0};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "pop") == 0) {
            long long valor = 0;
            if (desenfileirar(&fila, &valor)) {
                printf("pop %lld\n", valor);
            } else {
                printf("pop vazia\n");
            }
        } else {
            long long valor = strtoll(argv[i], NULL, 10);
            printf("push %lld %s\n", valor, enfileirar(&fila, valor) ? "ok" : "cheia");
        }
    }
    printf("tamanho=%d\n", fila.tamanho);
    return 0;
}
