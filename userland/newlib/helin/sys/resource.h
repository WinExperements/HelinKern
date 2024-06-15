#pragma once
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
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
typedef int rlim_t;
#define RLIM_INFINITY		-1
#define RLIM_SAVED_MAX		1024
#define RLIM_SAVED_CUR		0
#define RUSAGE_SELF		1
#define RUSAGE_CHILDREN		2
// Resource usage(Didn't implemented in kernel)
int getrlimit(int resource,struct rlimit *to);
int setrlimit(int resource,const struct rlimit *to);
struct rusage {
	struct timeval ru_utime;
	struct timeval ru_stime;
};
int getrusage(int resID,struct rusage *to);
#ifdef __cplusplus
}
#endif
