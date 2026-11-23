#include <stdio.h>
#include <stdlib.h>

static int vivos = 0;

static int *criar(int v) {
    int *p = malloc(sizeof(int));
    if (p == NULL) {
        exit(1);
    }
    *p = v;
    vivos += 1;
    return p;
}

static void liberar(int *p) {
    free(p);
    vivos -= 1;
}

int main(void) {
    int total = 0;
    for (int i = 1; i <= 3; i++) {
        int *p = criar(i);
        total += *p;
        liberar(p);
    }
    printf("%d\n", total);
    if (vivos != 0) {
        fprintf(stderr, "vazamento: %d blocos vivos\n", vivos);
        return 3;
    }
    return 0;
}
