#include <stdio.h>
#include <unistd.h>
#include <string.h>
#define MAX_LEN 100
extern FILE *stdin;

/* Я не знаю, як завантажувати імена та паролі користувачів, тому вони будуть статично записані */
typedef struct _user {
	char name[MAX_LEN];
	char password[MAX_LEN];
	int uid;
	int guid; // Використовуємо як і у UNIX!!!
	char *shell; // А що ж ми будемо виконувати коли ввійдемо? Правильно те що тут записано
} User;

static User users[] = {
	{
		"root",
		"toor",
		0,0,NULL
	},{
		"test",
		"password",
		1000,1000,"/initrd/shell"
	}
};

int main(int argc,char **argv) {
	char username[MAX_LEN];
	char password[MAX_LEN];
	bool logged = false;
	bool needUsername = true;
    int ppid = getppid();
	int numUsers = sizeof(users)/sizeof(users[0]);
	if (argc > 1) {
		// Нащо нам питати ім'я користувача? Ми і так його отримали з аргументів
		strcpy(username,argv[1]);
		needUsername = false;
	}
	User *user = NULL; // Користувач, в якого ит будемо входити
	printf("HelinOS login\r\n");
	while(!logged) {
		if (needUsername) {
			printf("Username: ");
			fread(username,1,100,stdin);
		}
		// Ми повинні вимикати відображення вводу, але поки що не реалізовано
		printf("Password: ");
		fread(password,1,100,stdin);
		// Перевіряємо!
		for (int i = 0; i < numUsers; i++) {
			if (strcmp(username,users[i].name) == 0 && strcmp(password,users[i].password) == 0) {
				// Ура! Користувач ввів все вірно!!!!!!!!!!!!!!!!!!!!!!!!!!
				user = &users[i]; // Не вбивайте будь ласка
			}
		}
		if (user == NULL) {
			printf("Login failed.\r\n");
		} else {
            // Користувач НІКОЛИ не вийде, УХАХХАХАХА
            setuid(user->uid);
	        // У нас немає оболонки, тому виходимо :(
            int pid = 0;
            if ((pid = execv("/initrd/sh",0,NULL)) > 0) {
                waitpid(pid,NULL,0);
                user = NULL; // Не забуваємо!
            } else {
                printf("Failed to execute shell!\n");
            }
        }
	}
	return 0;
}
