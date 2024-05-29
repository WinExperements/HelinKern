#pragma once
/*
 * IPC manager is special kernel side manager that provide easy userspace API to call various 
 * kernel IPC mechanism using not complicated functions and calls.
 *
 * This class is general and is standard kernel-side API for other IPC mechanisms for kernel modules.
 */ 
#include <thread.h>
typedef struct ipc_mech {
  char *name;
  int magicID; // This IPC mechanism can be at any position inside the list, find it using magic numbers.
  /* Arguments can be changed by target kernel code */
  int (*createIPC)(process_t *caller,void *args);
  int (*command)(process_t *caller,int cmd,void *args); // Comminunicate with IPC itself.
  int (*destroyIPC)(process_t *prc);
} Ipc;

void ipc_init(); // initialize IPC manager.
void ipc_init_standard(); // Initialize standard kernel side IPC mechanisms.
int ipc_add(Ipc *ipc);
int ipc_destroy(int magicID);
int ipc_remove(Ipc *ipc);
int ipc_create(int magicID,void *args);
int ipc_cmd(int magicID,int cmd,void *args);
int ipc_destroy(int magicID);
/* Unique Key generation */ 
void ipc_ftok(char *keyName,int flags);
