#ifndef SYS_SYSCALL_H
#define SYS_SYSCALL_H
#ifdef __cplusplus
extern "C" {
#endif
// Just define enum of system calls
enum syscall_table {
	SYS_default,
	SYS_print,
	SYS_exit,
	SYS_kill,
	SYS_getpid,
	SYS_stat,
	SYS_unlink,
	SYS_open,
	SYS_close,
	SYS_read,
	SYS_write,
	SYS_signal,
	SYS_sigexit,
	SYS_exec,
	SYS_reboot,
	SYS_link,
	SYS_pwd,
	SYS_chdir,
	SYS_opendir,
	SYS_closedir,
	SYS_readdir,
	SYS_mount,
	SYS_waitpid,
	SYS_getppid,
	SYS_sync,
	SYS_getuid,
	SYS_setuid,
	SYS_seek,
	SYS_tell,
	SYS_mmap,
	SYS_insmod,
	SYS_rmmod,
	SYS_ioctl,
	SYS_setgid,
	SYS_getgid,
	SYS_sbrk,
	SYS_dup,
	SYS_clone,
	SYS_waitForStart,
	SYS_usleep,
	SYS_truncate,
	SYS_socket,
	SYS_bind,
	SYS_listen,
	SYS_accept,
	SYS_connect,
	SYS_send,
	SYS_recv,
	SYS_ready,
	SYS_fork,
	SYS_uname,
	SYS_sethostname,
	SYS_gethostname,
	SYS_creat,
	SYS_munmap,
	SYS_umount,
	SYS_access,
	SYS_chmod,
	SYS_gettime,
	SYS_settime,
	SYS_syslog,
	SYS_chroot,
	SYS_fchdir,
	SYS_fchown,
	SYS_chown,
	SYS_rm,
	SYS_getpgid,
	SYS_nice,
	SYS_symlink,
  	SYS_ipc,
	SYS_getfsstat,	// BSD like syscall.
	SYS_getrlimit,
	SYS_setrlimit,
	SYS_getrusage,
	SYS_openat,
	SYS_sysconf,
	SYS_fcntl,
	NUM_SYSCALLS,
};

int syscall(int num,int p1,int p2,int p3,int p4,int p5); // helinkern specific syscall function
#ifdef __cplusplus
}
#endif
#endif
