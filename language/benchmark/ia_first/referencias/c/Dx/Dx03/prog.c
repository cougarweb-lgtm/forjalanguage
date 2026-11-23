#include <stdio.h>

typedef struct {
    int peso;
} Item;

static int peso_do(const Item *i) {
    if (i == NULL) {
        return 0;
    }
    return i->peso;
}

int main(void) {
    Item x = {5};
    printf("%d\n", peso_do(&x));
    printf("%d\n", peso_do(NULL));
    return 0;
}
