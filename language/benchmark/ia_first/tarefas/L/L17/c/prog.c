#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *dados;
    int tam;
    int cap;
} Lista;

static void anexar(Lista *l, int item) {
    if (l->tam == l->cap) {
        l->cap = l->cap * 2;
        l->dados = realloc(l->dados, (size_t)l->cap * sizeof(int));
        if (l->dados == NULL) {
            exit(1);
        }
    }
    l->dados[l->tam] = item;
    l->tam += 1;
}

int main(void) {
    Lista l = {malloc(2 * sizeof(int)), 0, 2};
    if (l.dados == NULL) {
        return 1;
    }
    for (int i = 1; i <= 5; i++) {
        anexar(&l, i * i);
    }
    printf("%d\n", l.tam);
    int s = 0;
    for (int i = 0; i < l.tam; i++) {
        s += l.dados[i];
    }
    printf("%d\n", s);
    printf("%d\n", l.dados[4]);
    free(l.dados);
    return 0;
}
