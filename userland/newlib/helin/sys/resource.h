#pragma once
/* Resource managment */
#define RLIMIT_CORE 		0
#define RLIMIT_CPU		1
#define RLIMIT_DATA		2
#define RLIMIT_FSIZE		3
#define RLIMIT_NOFILE		4
#define RLIMIT_STACK		5
#define RLIMIT_AS		6
struct rlimit {
	int rlim_cur;
	int rlim_max;
};
int getrlimit(int resource,struct rlimit *to);
int setrlimit(int resource,const struct rlimit *to);
