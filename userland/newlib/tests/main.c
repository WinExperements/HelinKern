#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

int main() {
	printf("HelinOS POSIX compatability all in one test suite.\r\n");
	printf("Testing chmod on tty\r\n");
	if (fchmod(0,777) < 0) {
		perror("operation failed");
	}
	// Unlink the tty(Must return the invalid argument error)
	printf("Testing unlink\r\n");
	int ret;
	if ((ret = unlink("/dev")) < 0) {
		perror("Unlink error(if errori is invalid argument, then test pass): ");
	}
	printf("return of unlink: %d\r\n",ret);
	printf("Test suite end.\r\n");
	return 0;
}
