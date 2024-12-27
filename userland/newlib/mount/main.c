#include <sys/mount.h>
#include <stdio.h>
int main(int argc,char **argv) {
	if (argc < 4) {
		fprintf(stderr,"mount <dev> <to> <fs>\n");
		return 1;
	}
	if (mount(argv[1],argv[2],argv[3],0,NULL) < 0) {
		perror("mount");
		return 1;
	}
	return 0;
}
