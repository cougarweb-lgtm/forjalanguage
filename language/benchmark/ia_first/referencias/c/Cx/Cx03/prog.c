#include <stdio.h>
#include <string.h>

static int tamanho(const char *nome) {
    return (int)strlen(nome);
}

int main(void) {
    printf("%d\n", tamanho("forja"));
    return 0;
}
