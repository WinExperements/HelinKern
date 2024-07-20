#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
int main(int argc,char **argv) {
	if (argc < 3) {
		fprintf(stderr,"Usage: chroot <new root path> <binary executable> <args>\r\n");
		return 1;
	}
	char *to = argv[1];
	char *exec = argv[2];
	char **newArgv = malloc(2+(argc - 3) * sizeof(char *));
	int a = 0;
	for (int i = 2,a = 0; i < argc; i++,a++) {
		newArgv[a] = argv[i];
	}
	newArgv[a] = NULL;
	if (chroot(to) < 0) {
		free(newArgv);
		perror("chroot");
		return 1;
	}
	// We in chroot, try to execute the executable.
	execv(argv[2],newArgv);
	free(newArgv);
	perror("execv");
	return 0;
}

