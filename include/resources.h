/* Resource limit tracking. */
#ifndef RES_H
#define RES_H
#define RLIMIT_CORE 		0
#define RLIMIT_CPU		1
#define RLIMIT_DATA		2
#define RLIMIT_FSIZE		3
#define RLIMIT_NOFILE		4
#define RLIMIT_STACK		5
#define RLIMIT_AS		6

typedef struct rlimit {
	int r_cur;
	int r_max;
} rlimit_t;
extern rlimit_t limitList[];
void krn_rlimit_init_default();
#endif
