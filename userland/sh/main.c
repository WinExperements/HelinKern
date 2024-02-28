#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "syscall.h"
#include "dirent.h"
#include "wait.h"

#define	USER_MMAP_START     	0x80000000


bool doexit = false;
char path[128];
int pid,ppid;
char buff[128];
void sh_parseCommand(char **argv,int argc);
bool execute(char *command,char **argv,int argc);
int __argc;
char **__argv;
void process_initScript(FILE *);
extern FILE *stdin;

typedef struct _pthread_str {
        int entry;
        void *arg;
} pthread_str;

typedef struct utsname {
    char *sysname;    /* Operating system name (e.g., "Linux") */
    char *nodename;   /* Name within communications network */
    char *release;
    char *version;
    char *machine;
};

// fork-exec model!
int fork() {
	return helin_syscall(49,0,0,0,0,0);
}

void th_main(void *str) {
    printf("Hi from thread! I am running from the same address space!\n");
    // Always exit
    // Other tests
    pthread_str *pt = (pthread_str *)str;
    printf("Structure dump: entry: %X, argument: %X\n",pt->entry,pt->arg);
    int clocks = 10;
    printf("REPEAT after 3 seconds\n");
    while(clocks != 0) {
	    printf("Repeat\n");
	    helin_syscall(39,3*100,0,0,0,0);
	    clocks--;
	}
    exit(0);
}

void fork_main(int argc,void *argv) {
	// ONLY TEST!
	printf("UNIX fork implemented!\n");
	exit(0);
}


int main(int argcf,char **argvf) {
    FILE *f = stdin;
    char buff[100];
    char *argv[100];
    while(doexit != true) {
        int argc = 0;
       printf(pwd(path,128));
       printf(" $ ");
       int len = fread(buff,1,90,f);
       buff[len-1] = 0;
       argv[argc] = strtok(buff," ");
       while(argv[argc]) {
        argc++;
        argv[argc] = strtok(NULL," ");
       }
       if (argc > 0) {
        sh_parseCommand(argv,argc);
       }
    }
    return 0;
}

void sh_parseCommand(char **argv,int argc) {
    if (argv[0][0] == '#') return;
    if (!strcmp(argv[0],"reboot")) {
        helin_syscall(14,0,0,0,0,0);
    } else if (!strcmp(argv[0],"poweroff")) {
        helin_syscall(14,0xfffc04,0,0,0,0);
    } else if (!strcmp(argv[0],"clear")) { 
	    int res = 0;
	    ioctl(0,3,&res);
    } else if (!strcmp(argv[0],"exit")) {
        doexit = true;
    } else if (!strcmp(argv[0],"cd")) {
        if (argc > 1) {
            chdir(argv[1]);
        }
    } else if (!strcmp(argv[0],"ls")) {
        DIR *d = NULL;
        if (argc >1) {
            d = opendir(argv[1]);
        } else {
            d = opendir(path);
        }
        if (d <= 0) {
		    printf("%s: no such file or directory\n",(argc > 1 ? argv[1] : path));
		    return;
	    }
        struct dirent *di = NULL;
        while((di = readdir(d)) != 0) {
            printf("%s ",di->name);
        }
        closedir(d);
        printf("\n");
    } else if (!strcmp(argv[0],"fault")) {
        uint32_t *a = (uint32_t *)0xf0000000;
        uint32_t aa = *a;
    } else if (!strcmp(argv[0],"mpe")) {
        helin_syscall(21,0,0,0,0,0);
        waitpid(pid,NULL,0);
    } else if (!strcmp(argv[0],"args")) {
        printf("arguments: %u\n",__argc);
        for (int i = 0; i < __argc; i++) {
            printf("%s\n",__argv[i]);
        }
    } else if (argv[0][0] == '/') {
        if (!execute(argv[0],argv,argc)) {
		printf("%s: no such file or directory\n",argv[0]);
	}
    } else if (!strcmp(argv[0],"help")) {
        printf("HelinOS userspace shell version 0.3\n");
        printf("reboot - reboot system\n");
        printf("poweroff - shutdown the emulator or halt the system\n");
        printf("clear - you know.\n");
        printf("exit - exit\n");
        printf("cd - change directory\n");
        printf("ls - list directory\n");
        printf("fault - only debug!\n");
        printf("mpe - didn't work\n");
        printf("args - print arguments information passed to the program\n");
    } else if (!strcmp(argv[0],"cli")) {
        printf("WARRNING: this command must only check the user task working via invoking privileged instruction!!\n");
        asm volatile("cli");
        // IF we here, so user mode are fake
    } else if (!strcmp(argv[0],"kill")) {
        if (argc == 1) {
            printf("kill - internal shell command\nuse kill <pid>\n");
            return;
        }
        int pid = atou(argv[1]);
        helin_syscall(3,pid,0,0,0,0);
    } else if (!strcmp(argv[0],"sysinfo")) {
        printf("Custom Shell for HelinOS\r\n");
        printf("UID: %u, GID: %u\r\n",getuid(),getgid());
        struct utsname *sys = malloc(sizeof(struct utsname));
        memset(sys,0,sizeof(struct utsname));
        helin_syscall(50,(int)sys,0,0,0,0);
        printf("OS: %s\n",sys->sysname);
        printf("Release: %s\n",sys->release);
        printf("Version: %s\n",sys->version);
        printf("Running on %s PC\n",sys->machine);
        free(sys);
    } else if (!strcmp(argv[0],"id")) {
        int uid = getuid();
        printf("UID: %u\n",uid);
    } else if (!strcmp(argv[0],"setuid")) {
        if (argc > 1) {
            int uid = atou(argv[1]);
            setuid(uid);
        } else {
            printf("setuid <uid>\n");
        }
    } else if (!strcmp(argv[0],"echo")) {
	    if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			printf("%s ",argv[i]);
		}
		printf("\n");
	    }
    } else if (!strcmp(argv[0],"insmod")) {
	if (argc > 1) {
		int ret = helin_syscall(30,(int)argv[1],0,0,0,0);
		if (ret == -2) {
			printf("Module %s loading failed please see console for more details\n",argv[1]);
		} else if (ret == -1) {
			printf("Fiie %s not found and can't be loaded!\n",argv[1]);
		}
	} else {
		printf("insmod - builtin function for installing and running kernel modules/drivers from shell\n");
		printf("Usage:	insmod <module path>\n");
	}
    } else if (!strcmp(argv[0],"mountfat")) {
        int ret = helin_syscall(21,(int)"/dev/hdap0",(int)"/fat",(int)"ext2",0,0);
    } else if (!strcmp(argv[0],"cat")) {
        if (argc > 1) {
            FILE *init_sh = fopen(argv[1],"r");
            if (init_sh <= 0) {
                printf("cat: %s: not found\r\n",argv[1]);
                return;
            }
            fseek(init_sh,0,SEEK_END);
	        int offset = ftell(init_sh);
	        fseek(init_sh,0,SEEK_SET);
            if (offset == 0) {
		        printf("Zero size\n");
                fclose(init_sh);
                return;
            }
            if (offset < 0) {
                printf("WTF?\r\n");
                fclose(init_sh);
                return;
            }
	        // now try to read it!
	        char *buf = helin_syscall(35,offset,0,0,0,0);
	        fread(buf,offset,1,init_sh);
	        fclose(init_sh);
            	printf("%s",buf);
            free(buf);
        }
    } else if (!strcmp(argv[0],"clone")) {
        helin_syscall(37,(int)th_main,0x200,10,0,0);
    } else if (!strcmp(argv[0],"fork")) {
	// Fork
	int ret = helin_syscall(49,(int)fork_main,0,0,0,0);
	if (ret == -1) {
		printf("fork failed\n");
	} else {
		printf("Fork return: %u\n",ret);
        int ud = getpid();
        printf("My PID: %u, child PID: %u\n",ud,ret);
        if (ret == 0) {
                fork_main(0,NULL);
        } else {
            printf("I AM NOT A CHILD!\n");
        }
	}
    } else if (!strcmp(argv[0],"graphics")) {
	    char *parallelArgv[] = {"nano-X","&"};
	    if (!execute("nano-X",parallelArgv,2)) {
		    printf("No Nano-X found :(\n");
		    return;
	    }
	    execute("demo-hello",parallelArgv,2);
	    execute("demo-grabkey",parallelArgv,2);
    } else if (!strcmp(argv[0],"hdd")) {
        printf("Hard Disk Test Suite utility\n");
        FILE *n = fopen("/dev/hda","r");
        if (n == -1) {
            printf("Failed to open first detect hard drive! (Is the ATA driver correctly work?)\n");
            return;
        }
        printf("Select choise\n");
        printf("a. Spin off first detected hard drive\n");
        printf("b. Spin on first detected hard drive\n");
        printf("c. Read sector 20 of first detected hard drive.\n");
        char sel[2];
        fread(&sel,2,1,stdin);
        if (sel[0] == 'a') {
        	if (ioctl(n,0x10) < 0) {
        		printf("HDD spin off fail\n");
        	}
        } else if (sel[0] == 'b') {
        	if (ioctl(n,0x20) < 0) {
        		printf("HDD spin on fail\n");
        	}
        } else if (sel[0] == 'c') {
        	printf("Okay, okay,reading, about 200M of disk\r\n");
        	int sectorsIn200M = (200*1024*1024) / 512;
        	int iterations = sectorsIn200M / 255;
        	printf("Sectors in 200M: %u, iterations: %u\r\n",sectorsIn200M,iterations);
        	char *bufa = malloc(512*255);
        	for (int i = 0; i < iterations; i++) {
        		fread(bufa,512*255,1,n);
        		if (i % 10 == 0 && i != 0) {
        			printf("Passed %u iterration of reading!\r\n",i);
        		}
        	}
        	printf("\n");
        	free(bufa);
        }
         fclose(n);
    } else {
        if (!execute(argv[0],argv,argc)) {
            printf("Commmand %s not found\n",argv[0]);
        }
    }
}
bool execute(char *command,char **argv,int argc) {
    char *run_path = "/initrd";
    int _pid = 0;
    char **new_argv = NULL;
    int new_argc = argc-1;
    if (run_path == NULL) {
        printf("cannot find PATH enviroment variable!\n");
        return false;
    }
    bool parallel = false;
    if (argc > 1) {
        // check if we need to execute it paralell
        if (!strcmp(argv[argc-1],"&")) {
            //printf("parallel running\n");
	        parallel = true;
            argc--;
        }
	int newArgvSize = sizeof(char *)*argc;
        new_argv = malloc(newArgvSize);
	memset(new_argv,0,newArgvSize);
        for (int i = 0; i < new_argc; i++) {
	    //printf("%u %s\n",i,new_argv[i]);
            new_argv[i] = argv[i];
        }
	//printf("u %u\n",new_argc);
    }
    // if path is absolute doesn't call sprinf
    if (command[0] != '/') {
    	sprintf(buff,"%s/%s",run_path,command);
    } else {
	    strcpy(buff,command);
    }
    // fork then execute
    _pid = fork();
    if (_pid == 0) {
	    execv(buff,new_argc,new_argv);
	    // error :(
	    printf("%s: failed to execute!\n",buff);
	    exit(1);
    }
    if (_pid != 0) {
        if (!parallel) {
            waitpid(_pid,NULL,0);
            if (new_argv)free(new_argv);
        } else {
            printf("[%u]\r\n",_pid);
        }
        return true;
    } else {
	    printf("PID: %u\n",_pid);
        if (new_argv)free(new_argv);
        return false;
    }
    return false;
}
