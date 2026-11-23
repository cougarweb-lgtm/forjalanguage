#include <stdio.h>

typedef struct {
    int x;
    int y;
} Ponto;

int main(void) {
    Ponto p = {3, 4};
    int n = p;
    printf("%d\n", n);
    return 0;
}
