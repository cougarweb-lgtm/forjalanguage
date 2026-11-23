#include <stdio.h>
#include <stdlib.h>

static int *criar(void) {
    int *p = malloc(sizeof(int));
    *p = 7;
    return p;
}

int main(void) {
    int *p = criar();
    printf("%d\n", *p);
    free(p);
    return 0;
}
