#include <assert.h>
#include <stdio.h>

static int media(int a, int b) {
    return a + b / 2;
}

int main(void) {
    assert(media(10, 20) == 15);
    printf("ok\n");
    return 0;
}
