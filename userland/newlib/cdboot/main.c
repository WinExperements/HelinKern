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
void retryAnotherDisk() {
	printf("It's looks like the CD that in the medium does not contain required files/software or broken. We will open the tray for you, please insert correct optical drive that is formatted within file system ISO9660 and contains HelinOS CD/DVD disk applications.\r\n");
	int fd = open("/dev/satapi0",O_RDWR);
	if (fd < 0) {
		printf("It's looks like you disconnect the optical drive. Cannot continue boot process\r\n");
		while(1) {}
	}
	int cmd = 0x1b;
	ioctl(fd,0x31,cmd);
	printf("Press Enter when you are ready\r\n");
	char buff[3];
	read(0,buff,2);
	close(fd);
}

int main() {
	printf("Welcome to CDBOOT!\r\n");
	printf("This program will help you to prepare the rest of the boot process from your CDROM drive\r\n");
	printf("Warrning: Currently HelinOS doesn't support any kind of removable media except for the CD/DVD disks\r\n");
	printf("Warrning: Currently only AHCI(SATA) CDROM's supported. If you have enabled legacy emulation in your BIOS settings, reboot and change again to AHCI, also some firmware vendors removes the communication between the controller and physical port, you can check it by finding the hard drive/CDROM detection messages, if you don't see any kind of CDROM detection messages, then your firmware likely disconnect this kind of connection, in this scenario the live cd is impossible.\r\n");
	// Check the ATAPI device existance.
	char *mntP = strdup("/dev/satapi0");
	if (access("/dev/satapi0",F_OK) < 0) {
		printf("Cannot find the Optical Drive. The boot cannot be continued\r\n");
		while(1) {}
	}
	int i = 0;
	printf("Trying to mount: %s\r\n",mntP);
retryMount:
	for (i = 0; i < 3; i++) {
		if (mount(mntP,"/mnt","iso9660",0,NULL) == 0) {
			break;
		}
		i++;
	}
	if (i == 3) {
		printf("Failed to mount current medium in ATAPI drive 0\r\n");
		retryAnotherDisk();
		goto retryMount;
	}
	// Uppon successful, try to find the correct file.
	if (access("/mnt/usr/bin/wm",F_OK) < 0) {
		printf("This media doesn't contain required LIVE CD files\r\n");
		retryAnotherDisk();
		goto retryMount;
	}
	printf("cdboot: user successfully prepared boot environment, booting rest of software from external devices\r\n");
	int child = fork();
	if (child == 0) {
		char *app_argv[] = {"/mnt/usr/bin/wm",NULL};
		char *app_envp[] = {"PATH=/bin:/mnt/usr/bin",NULL};
		if (execve("/mnt/usr/bin/wm",app_argv,app_envp) < 0) {
			perror("Failed to execute rest of the software");
			return 1;
		}
	} else {
		int status;
		waitpid(child,&status,0);
		if (status == 1) {
			retryAnotherDisk();
			goto retryMount;
		}
	}
	return 0;
}
	
