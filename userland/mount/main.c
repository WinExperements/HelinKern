#include "stdio.h"
#include "syscall.h"
int main(int argc,char **argv) {
    if (argc < 2) {
	printf("%u\n",argc);
        printf("mount: <fs type> <device path> <mount point>\n");
        return 0;
    }
    printf("%s %s %s\n",argv[1],argv[2],argv[3]);
    int ret = helin_syscall(21,(int)argv[2],(int)argv[3],(int)argv[1],0,0);
    if (ret == -2) {
        printf("mount: %s: no such device\n",argv[2]);
        return 1;
    } else if (ret == -3) {
        printf("mount: %s: no such directory\n",argv[3]);
        return 1;
    } else if (ret == -4) {
        printf("mount: %s: no such FS\n",argv[1]);
        return 1;
    } else if (ret == -5) {
	    printf("mount: %s: mount failed\n");
    }
    return 0;
}
