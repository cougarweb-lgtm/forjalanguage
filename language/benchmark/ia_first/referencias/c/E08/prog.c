#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    int limite = atoi(argv[1]);
    int fizzbuzz = 0;
    for (int i = 1; i <= limite; i++) {
        if (i % 15 == 0) {
            printf("FizzBuzz\n");
            fizzbuzz++;
        } else if (i % 3 == 0) {
            printf("Fizz\n");
        } else if (i % 5 == 0) {
            printf("Buzz\n");
        } else {
            printf("%d\n", i);
        }
    }
    return fizzbuzz;
}
