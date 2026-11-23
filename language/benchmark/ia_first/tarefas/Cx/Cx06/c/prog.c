#include <stdio.h>

static int area(int largura, int altura) {
    return largura * altura;
}

int main(void) {
    printf("%d\n", area(6, 7, 2));
    return 0;
}
