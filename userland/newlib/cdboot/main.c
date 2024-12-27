/*
 * CDBOOT - Initramfs mini program to prepare system startup from CDROM
 * using easy text messages to help user understood the process(Maybe)
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
static char devpath[100];
void tryDrive(char *drive) {
	snprintf(devpath,100,"/dev/%s",drive);
	printf("Checking for device: %s\n",devpath);
	if (access(devpath,F_OK) < 0) {
		printf("Cannot find the Optical Drive. The boot cannot be continued\r\n");
		while(1) {}
	}
	int i = 0;
	printf("Trying to mount: %s\r\n",devpath);
retryMount:
	for (i = 0; i < 3; i++) {
		if (mount(devpath,"/mnt","iso9660",0,NULL) == 0) {
			break;
		}
		i++;
	}
	if (i == 3) {
		printf("Failed to mount current medium in ATAPI drive 0\r\n");
		return;
	}
	// Uppon successful, try to find the correct file.
	if (access("/mnt/init",F_OK) < 0) {
		printf("This media doesn't contain required LIVE CD files\r\n");
		return;
	}
	// Try to chroot.
	if (chroot("/mnt") < 0) {
		perror("chroot");
	}
	printf("cdboot: user successfully prepared boot environment, booting rest of software from external devices\r\n");
	char *app_argv[] = {"/init",NULL};
	char *app_envp[] = {"PATH=/bin:/sbin",NULL};
	if (execve("/init",app_argv,app_envp) < 0) {
		perror("execute error");
	}
}
int main() {
	char *hostname = "localhost";
	int hostlen = strlen(hostname);
	sethostname(hostname,hostlen);
	// Preapre some things.
	if (mount("/dev","/dev","devfs",0,NULL) < 0) {
		perror("Failed to mount devfs.");
		while(1) {}
	}
//	ioctl(0,3);
        printf("Tying to install AHCI driver\n");
        if (syscall(SYS_insmod,(uintptr_t)"/lib/modules/fat32.mod",0,0,0,0)) {
                perror("insmod");
        }
                if (syscall(SYS_insmod,(uintptr_t)"/lib/modules/ahci.mod",0,0,0,0)) {
                perror("ahci insmod");
        }
        syscall(SYS_ipc,2,'P',2,NULL,0);
	printf("Welcome to CDBOOT!\r\n");
	printf("This program will help you to prepare the rest of the boot process from your CDROM drive\r\n");
	printf("Warrning: Currently HelinOS doesn't support any kind of removable media except for the CD/DVD disks\r\n");
	printf("Scanning for IDE/SATA CD-DRIVEs\n");
	DIR *d = opendir("/dev/");
	if (!d) {
		perror("Failed to open dev\n");
		while(1) {}
	}
	struct dirent *t = NULL;
	while((t = readdir(d)) != NULL) {
		if (strncmp(t->d_name,"cd",2) == 0) {
			printf("Found IDE CD: %s\n",t->d_name);
			tryDrive(t->d_name);
		} else if (strncmp(t->d_name,"satapi",6) == 0) {
			printf("Found SATA IDE\n");
			tryDrive(t->d_name);
		}
	}
	closedir(d);
	printf("No IDE drives found OR the drive has invalid disk\n");
	while(1) {}
	return 0;
}

