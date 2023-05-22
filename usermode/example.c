#include <stdio.h>

int fun1(int a, int b) {
    return a > b;
}

void fun2() {

}


int main(void) {
    printf("main addr: %llx\n", main);
    printf("fun1 addr: %llx\n", fun1);

    while (1) {
        int a = 1, b = 2;
        printf("fun1(%i, %i): %i\n", a, b, fun1(a, b));
        printf("fun1 disas: ");
        for (int i = 0; i < (long) fun2 - (long) fun1; i++) {
            char c = ((char *) fun1)[i];
            printf("%hhx ", c);
        }
        printf("\n");
        sleep(1);
    }
}
