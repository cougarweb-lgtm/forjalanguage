#include <stdio.h>
#include <stdlib.h>

static int *criar(void) {
    int x = 7;
    return &x;
}

int main(void) {
    int *p = criar();
    printf("%d\n", *p);
    free(p);
    return 0;
}
