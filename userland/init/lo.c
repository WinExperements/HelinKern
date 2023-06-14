#include <stdio.h>

int main(int argc,char **argv) {
    printf("a");
    int i = 0;
    int cycles = 0;
    for (;;) {
    if (i == 20000) {
            printf("b");
            i = 0;
             if (cycles == 10000) {
            break;
        }
        cycles++;
        }
        i++;
    }
    return 0;
}
