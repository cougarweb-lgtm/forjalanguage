#include <stdio.h>

typedef struct {
    int vida;
    int escudo;
} Nave;

int main(void) {
    Nave n = {10, 5};
    printf("%d\n", n.vida + n.escudo);
    return 0;
}
