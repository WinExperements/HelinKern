/*
 * HelinOS init system commands.
*/
#include <stdio.h>
#include <sys/reboot.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
int main(int argc,char **argv) {
        int force = 0;
        if (argc > 1 && (!strcmp(argv[1],"-f"))) {
                force = 1;
        }
#if defined(INIT_POWEROFF)
        if (force) {
                if (reboot(RB_POWER_OFF) < 0) {
                        perror("poweroff failed");
                        return 1;
                }
        } else {
                kill(1,SIGINT);
        }
#elif defined(INIT_REBOOT)
        if (force) {
                if (reboot(RB_AUTOBOOT) < 0) {
                        perror("reboot failed");
                        return 1;
                }
        } else {
                kill(1,SIGUSR1);
        }
#else
        printf("%s: unknown build type!\n",argv[0]);
#endif
        return 0;
}

