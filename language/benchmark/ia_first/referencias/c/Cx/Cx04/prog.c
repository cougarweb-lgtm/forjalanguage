#include <stdio.h>

int main(void) {
    double d = 2.5;
    double *p = &d;
    printf("%d\n", (int)*p);
    return 0;
}
