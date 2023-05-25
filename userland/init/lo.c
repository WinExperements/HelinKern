#include "stdio.h"

int main(int argc,char **argv) {
    asm volatile("sti");
    for (int i = 0; i < 40; i++) {
        printf("A");
    }
    return 0;
}
