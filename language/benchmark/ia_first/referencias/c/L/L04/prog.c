#include <stdio.h>

typedef struct {
    int v[4];
} Quatro;

int main(void) {
    Quatro a = {{1, 2, 3, 4}};
    Quatro b = a;
    b.v[0] = 99;
    a.v[3] = 50;
    printf("%d\n", a.v[0] + a.v[3]);
    printf("%d\n", b.v[0] + b.v[3]);
    return 0;
}
