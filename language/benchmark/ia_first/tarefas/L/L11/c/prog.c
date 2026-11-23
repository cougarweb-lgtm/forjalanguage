#include <stdio.h>
#include <string.h>

int main(void) {
    const char *a = "ola";
    const char *b = "ação";
    printf("%d\n", (int)strlen(a));
    printf("%d\n", (int)strlen(b));
    printf("%s\n", strcmp(a, "ola") == 0 ? "true" : "false");
    printf("%s\n", strcmp(b, "acao") == 0 ? "true" : "false");
    printf("%d\n", (int)strlen(b) - (int)strlen(a));
    return 0;
}
