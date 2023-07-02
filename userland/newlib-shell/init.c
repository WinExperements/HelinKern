#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


int main() {
	// Just autostart the shn process
	char *args[] = {"hi",NULL};
	int pid = execvp("/initrd/shn",args);
	if (pid > 0) {
		waitpid(pid,NULL,0);
	}
	return 0;
}
