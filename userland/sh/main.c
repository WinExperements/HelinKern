#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "syscall.h"
#include "dirent.h"
#include "wait.h"


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


int main(int argcf,char **argvf) {
    FILE *f = stdin;
    char buff[100];
    char *argv[100];
    while(doexit != true) {
        int argc = 0;
       printf(pwd(path,128));
       printf(" $ ");
       fread(buff,1,100,f);
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
        printf("\033[2J"); // only test, must work
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
        if (!d) {
		    printf("%s: I/O error\n",(argc > 1 ? argv[1] : path));
		    return;
	    }
        struct dirent *di = NULL;
        while((di = readdir(d)) != 0) {
            printf("%s\n",di->name);
        }
        closedir(d);
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
        int _pid = 0;
        if ((_pid = execv(argv[0],0,NULL)) > 0) {
            // wait
            //printf("shell: spawned process: %u\n",_pid);
            waitpid(_pid,NULL,0);
        } else {
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
        helin_syscall(24,0,0,0,0,0);
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
        int ret = helin_syscall(21,(int)"/dev/hdap0",(int)"/initrd",(int)"fat32",0,0);
    } else if (!strcmp(argv[0],"cat")) {
        if (argc > 1) {
            FILE *init_sh = fopen(argv[1],"r");
            if (init_sh < 0) {
                printf("cat: %s: not found\r\n",argv[1]);
                return;
            }
            fseek(init_sh,0,SEEK_END);
	        int offset = ftell(init_sh);
	        fseek(init_sh,0,SEEK_SET);
            if (offset == 0) {
                fclose(init_sh);
                return;
            }
	        // now try to read it!
	        char *buf = malloc(offset);
	        fread(buf,offset,1,init_sh);
	        fclose(init_sh);
            printf("%s",buf);
            free(buf);
        }
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
        new_argv = malloc(100);
        for (int i = 0; i < argc; i++) {
            new_argv[i] = argv[i];
        }
	//printf("u %u\n",new_argc);
    }
    sprintf(buff,"%s/%s",run_path,command);
    if ((_pid = execv(buff,argc,new_argv)) > 0) {
        if (!parallel) {
            waitpid(_pid,NULL,0);
            if (new_argv)free(new_argv);
        }
        return true;
    } else {
        if (new_argv)free(new_argv);
        return false;
    }
    return false;
}
