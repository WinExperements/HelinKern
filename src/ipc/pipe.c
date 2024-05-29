#include <ipc/pipe.h>
#include <ipc/ipc_manager.h>
#include <output.h>
#include <vfs.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <lib/fifosize.h>
// VFS methods.
static int pipe_vfs_read(vfs_node_t *node,uint64_t start,uint64_t size,void *buffer);
static int pipe_vfs_write(vfs_node_t *node,uint64_t start,uint64_t size,void *buffer);
static void pipe_vfs_close(vfs_node_t *node);
// IPC methods :)
typedef struct pipe_data {
  process_t *creator;
  queueSize *queue;
  int origDataSize;
  int read_fd,write_fd;
} PipeData;
static vfs_fs_t *pipe_fs;
int pipe_create(process_t *caller,void *args) {
  // args - fd[]
  if  (caller == NULL || args == NULL) return -1;
  vfs_node_t *reader = kmalloc(sizeof(vfs_node_t));
  vfs_node_t *writer = kmalloc(sizeof(vfs_node_t));
  memset(reader,0,sizeof(vfs_node_t));
  memset(writer,0,sizeof(vfs_node_t));
  PipeData *data = kmalloc(sizeof(PipeData));
  memset(data,0,sizeof(PipeData));
  data->creator = caller;
  data->queue = queueSize_create(64*1024);
  reader->fs = pipe_fs;
  writer->fs = pipe_fs;
  reader->priv_data = data;
  writer->priv_data = data;
  writer->inode = 1;
  // Populate the fds.
  int *fds = (int *)args;
  fds[0] = thread_openFor(caller,reader);
  fds[1] = thread_openFor(caller,writer);
  data->write_fd = fds[1];
  data->read_fd = fds[0];
  return 0;
}
int pipe_destroyIPC(process_t *caller) {
  return 0;
}
// vfs functions.
static int pipe_vfs_write(vfs_node_t *node,uint64_t start,uint64_t size,void *buffer) {
  // Determinate what kind of file we are(reader or writer.)
  PipeData *data = (PipeData *)node->priv_data;
  return (int)queueSize_enqueue(data->queue,(uint8_t *)buffer,(uint32_t)size);
}
static int pipe_vfs_read(vfs_node_t *node,uint64_t start,uint64_t size,void *buffer) {
  PipeData *data = (PipeData *)node->priv_data;
  if ((node->flags & VFS_NONBLOCK) != VFS_NONBLOCK) {
    while(queueSize_is_empty(data->queue));
  }
  return queueSize_dequeue(data->queue,(char *)buffer,(uint32_t)size);
}
static void pipe_vfs_close(vfs_node_t *node) {
  // Did we read or write node type?
  PipeData *data = (PipeData *)node->priv_data;
  if (node->inode == 1) {
    data->write_fd = 0;
  } else {
    data->read_fd = 0;
  }
  if (data->write_fd == 0 && data->read_fd == 0) {
    // Clean up resources.
    kprintf("pipe: Cleaning up resources due to closing up of the pipe itself\r\n");
    queueSize_destroy(data->queue);
    kfree(data);
  }
  kfree(node);
}
void pipe_init() {
  // Register fs.
  pipe_fs = kmalloc(sizeof(vfs_fs_t));
  memset(pipe_fs,0,sizeof(vfs_fs_t));
  pipe_fs->read = pipe_vfs_read;
  pipe_fs->write = pipe_vfs_write;
  pipe_fs->close = pipe_vfs_close;
  // IPC itself.
  Ipc *pipe = kmalloc(sizeof(Ipc));
  memset(pipe,0,sizeof(Ipc));
  pipe->createIPC = pipe_create;
  pipe->destroyIPC = pipe_destroyIPC;
  pipe->name = "pipe";
  pipe->magicID = PIPE_MAGIC;
  ipc_add(pipe);
  kprintf("pipe IPC registred with magic: 0x%x\r\n",PIPE_MAGIC);
}
