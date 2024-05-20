#include <ipc/ipc_manager.h>
#include <lib/queue.h> // why not.
#include <ipc/pipe.h>
#include <output.h>
queue_t *registredIPC;
void ipc_init() {
  registredIPC = queue_new();
}
static Ipc *findIPC(int magic) {
  queue_for(element,registredIPC) {
    Ipc *ipc = (Ipc *)element->value;
    if (!ipc) continue;
    if (ipc->magicID == magic) {
      return ipc;
    }
  }
  return NULL;
}
void ipc_init_standard() {
  pipe_init();
}
int ipc_add(Ipc *ipc) {
  // Check if we not registred it before.
  if (!ipc) return -1;
  if (findIPC(ipc->magicID) != NULL) {
    kprintf("ipc_manager: IPC mechanism with name %s already registred inside kernel\r\n",ipc->name);
    return -1;
  }
  // Just place into queue.
  enqueue(registredIPC,ipc);
}
int ipc_destroy(int magic) {
  Ipc *ms = findIPC(magic);
  if (!ms) return -1;
  if (!ms->destroyIPC) return -1;
  process_t *prc = thread_getThread(thread_getCurrent());
  return ms->destroyIPC(prc);
}
int ipc_remove(Ipc *ipc) {
  if (ipc == NULL) return -1;
  queue_remove(registredIPC,ipc);
  return 0;
}
int ipc_create(int magicID,void *args) {
  // Find IPC itself.
  process_t *prc = thread_getThread(thread_getCurrent());
  Ipc *ipc = findIPC(magicID);
  if (!ipc) return -1;
  if (!ipc->createIPC) return -1;
  return ipc->createIPC(prc,args);
}
int ipc_cmd(int magicID,int cmd,void *args) {
  process_t *prc = thread_getThread(thread_getCurrent());
  Ipc *ipc = findIPC(magicID);
  if (!ipc) return -1;
  if (!ipc->command) return -1;
  return ipc->command(prc,cmd,args);
}
