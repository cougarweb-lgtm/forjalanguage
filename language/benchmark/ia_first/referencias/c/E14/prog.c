#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *nome = getenv("TAREFA_NOME");
    printf("Ola, %s!\n", nome != NULL ? nome : "desconhecido");
    return 0;
}
