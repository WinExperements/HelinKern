#ifndef SYS_SHM_H
#define SYS_SHM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#define SHM_W	10
#define SHM_R	11
typedef int shmatt_t;
struct shmid_ds {
	size_t          shm_segsz;  //Size of segment in bytes. 
	pid_t           shm_lpid;   //Process ID of last shared memory operation. 
	pid_t           shm_cpid;   //Process ID of creator. 
	shmatt_t        shm_nattch; //Number of current attaches. 
	time_t          shm_atime;  //Time of last shmat
	time_t          shm_dtime;  //Time of last shmdt
	time_t          shm_ctime;  //Time of last change by shmctl
};
void *shmat(int shmid, const void *to, int flags);
int   shmctl(int shmid, int cmd, struct shmid_ds *ds);
int   shmdt(const void *ptr);
int   shmget(key_t key, size_t size, int shmflags);


// C++ end.
#ifdef __cplusplus
}
#endif
#endif
