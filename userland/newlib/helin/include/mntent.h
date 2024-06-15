#ifndef MNTENT_H
#define MNTENT_H

#define _PATH_MOUNTED "/proc/mounts"

struct mntent
  {
    char *mnt_fsname;           /* Device or server for filesystem.  */
    char *mnt_dir;              /* Directory mounted on.  */
    char *mnt_type;             /* Type of filesystem: ufs, nfs, etc.  */
    char *mnt_opts;             /* Comma-separated options for fs.  */
    int mnt_freq;               /* Dump frequency (in days).  */
    int mnt_passno;             /* Pass number for `fsck'.  */
  };

extern "C" {
	extern struct mntent *getmntent (FILE *__stream);
}

#endif
