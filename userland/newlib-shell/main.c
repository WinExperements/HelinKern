// Shell program that use newlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
char path[100];

void parse(int argc,char **argv);

int main() {
    char line[100];
    bool exit = false;
    int argc = 0;
    char *argv[100];
    while(!exit) {
        //putc('>',stdout);
        getcwd(path,100);
        printf("[%s]> ",path);
        fflush(stdout);
        fgets(line,100,stdin);
        line[strcspn(line,"\n")] = 0;
        if (!strcmp(line,"exit")) {
            exit = true;
            break;
        }
        argv[argc] = strtok(line," ");
        while(argv[argc]) {
            argc++;
            argv[argc] = strtok(NULL," ");
        }
        if (argc > 0) {
            parse(argc,argv);
            argc = 0;
        }
    }
    return 0;
}

void parse(int argc,char **argv) {
    if (!strcmp(argv[0],"ls")) {
        DIR *d = opendir(path);
        if (!d) {
            printf("Failed to open %s\n",path);
            return;
        }
        struct dirent *dir = readdir(d);
        while(dir) {
            printf("%s ",dir->d_name);
            fflush(stdout);
            dir = readdir(d);
        }
        closedir(d);
        printf("\n");
    } else {
        printf("Unknown command: %s\n",argv[0]);
    }
}