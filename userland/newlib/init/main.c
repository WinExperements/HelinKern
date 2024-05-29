/*
 * UNIX compatable systems init script.
 * Originally writen for HelinOS.
 * Also ported to: Linux.
 * Written by: WinExperements.
 * Required things to boot this init system:
 * - Some kind of POSIX support.(Not full support are needed)
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <sys/reboot.h>
// Use uname() for the system identification.

// OS-specific defines here.
#define CONF_PATH "/init.rc"

void printVersion() {
	struct utsname osID;
	uname(&osID);
	printf("HelinOS init system, version 0.1, builded for %s, using GCC %s\r\n",osID.sysname,__VERSION__);
}

void parseCommand(int argc,char **argv);
void emergExit(); // exit on unrecoverable error.
// Yes. Like shell commands, but parsed by internal parser.

// Service control structure.

typedef struct serv {
	char *serviceName;
	char *execPath;
	int priority;
	char **prc_argv;
	int pid;
	int executionRetries; // if reach more that 4, force disable the service
	struct serv *next;
} service_t;

static service_t *globalServiceList;
extern char **environ;
void sigint_handler(int sig);
int main(int argc,char **argv) {
	int mypid = getpid();
	if (mypid != 1) {
		fprintf(stderr,"The init system can be runned only as PID 1!\r\n");
		return 1;
	}
	printVersion();
  	signal(SIGINT,sigint_handler);
	printf("Configuration path: %s\r\n",CONF_PATH);
	int rc_fd = open(CONF_PATH,O_RDONLY);
	if (rc_fd < 0) {
		fprintf(stderr,"Failed to open the configuration file: %s, ",CONF_PATH);
		perror("error");
		emergExit();
	}
  	printf("init[1]: Setting up host name to \"localhost\"\r\n");
  	char *hostname = "localhost";
  	sethostname(hostname,sizeof(hostname));
 	 // Setup environment.
	setenv("PATH","/bin:/sbin",1);
	// Read init.rc file line by line.
	char buffer[1024];
	ssize_t readed;
	char *ptr = NULL;
	int cmd_argc = 0;
	char **cmd_argv = malloc(100);
	if (cmd_argv == NULL) {
		printf("Cannot allocate argv memory!\r\n");
		emergExit();
	}
	char *cmd_savePtr = NULL; // required by strtok_r
	while((readed = read(rc_fd,buffer,1024)) > 0) {
		ptr = strtok(buffer,"\n");
		while(ptr != NULL) {
			if (ptr[0] == '#') {
				// Comment. Skip
				goto end;
			}
			// Parse command.
			char *token = strtok_r(ptr," ",&cmd_savePtr);
			while(token != NULL) {
				cmd_argv[cmd_argc] = token;
				token = strtok_r(NULL," ",&cmd_savePtr);
				cmd_argc++;
			}
			parseCommand(cmd_argc,cmd_argv);
			end:
			cmd_argc = 0; // reset
			ptr = strtok(NULL,"\n");
		}
	}
	free(cmd_argv);
	close(rc_fd);
	printf("End of init system. Monitoring for system services.....bzzz......\r\n");
	// Main loop!
	while(1) {
		int dieStatus = 0;
		int diedChild = waitpid(-1,&dieStatus,0);
		if (diedChild > 0) {
			for (service_t *start = globalServiceList; start != NULL; start = start->next) {
				if (start->pid == diedChild) {
					if (start->executionRetries >= 4) {
						printf("Force disabling service %s due to 4 previous failed run attempts!\r\n");
						break; // breaks the for loop.
					} else {
						// We able to restart this crutical service. Limit reboots to 4.
						// Debug
						printf("init[1]: Restarting service %s.",start->serviceName);
						int restarted = fork(); // required to execute new processrs.
						if (restarted == 0) {
							execv(start->execPath,start->prc_argv);
							perror("init[1]: service restarting fauilure: ");
							exit(1);
						} else {
							start->pid = restarted;
						}
				}
			}
		}}
	}
	return 0;
}

void emergExit() {
	printf("!!!!	CRITICAL ERROR		!!!!\r\n");
	printf("The system run into unrecoverable error. The error message can be showed below before this message.\r\n");
	printf("The system can't operate. Halt\r\n");
	while(1) {}
}
void parseCommand(int argc,char **argv) {
	if (argc < 1) return;
	if (!strcmp(argv[0],"echo")) {
		for (int i = 1; i < argc; i++) {
			printf("%s ",argv[i]);
		}
		printf("\n");
	} else if (!strcmp(argv[0],"service")) {
		// service description format:
		// service name		priority(int)	executable path with arguments
		char *serviceName = argv[1];
		int priority = atoi(argv[2]);
		char *executable = argv[3];
		//printf("Start up service, name -> %s, priority -> %d, executable -> %s!\r\n",serviceName,priority,executable);
		// Build the argc and argv for a new process!
		char **new_argv = malloc(argc-3);
		int i,j = 0;
		for (i = 3,j = 0; i < argc, j < argc - 3; i++,j++) {
			new_argv[j] = argv[i];
		}
		new_argv[j] = NULL;
		int child = fork();
		if (child == 0) {
			// Now we can call, execv
			execv(new_argv[0],new_argv);
			// If we here, something went wrong.
			perror("Execution error: ");
			exit(1);
		}
		// register service
		service_t *service = malloc(sizeof(service_t));
		memset(service,0,sizeof(service_t));
		service->serviceName = serviceName;
		service->execPath = executable;
		service->priority = priority;
		service->prc_argv = new_argv;
		service->pid = child;
		if (globalServiceList == NULL) {
			globalServiceList = service;
		} else {
			// Place service into list
			service_t *start = globalServiceList;
			while(start->next != NULL) {
				start = start->next;
			}
			start->next = service;
		}
	} else if (!strcmp(argv[0],"insmod")) {
		if (argc < 2) {
			fprintf(stderr,"init: insmod require the module path!\r\n");
			return;
		}
		if (syscall(SYS_insmod,(int)argv[1],0,0,0,0) < 0) {
			fprintf(stderr,"init: insmod fail.\r\n");
			return;
		}
	} else if (!strcmp(argv[0],"mount")) {
		if (argc < 4) {
			printf("init: mount: not enought arguments\r\n");
			return;
		}
		if (mount(argv[1],argv[2],argv[3],0,NULL) < 0) {
			perror("mount operation fail");
		}
	}
}
void sigint_handler(int sig) {
  printf("init[1]: Perfoming system poweroff\r\n");
  printf("Shutting down system services....");
  fflush(stdout);
  for (service_t *srv = globalServiceList; srv != NULL; srv = srv->next) {
	printf("%s ",srv->serviceName);
	fflush(stdout);
	kill(srv->pid,SIGKILL);
  }
  printf("\r\nDone, unmounting all removable file systems...");
  // TODO
  fflush(stdout);
  reboot(RB_POWER_OFF);
}
