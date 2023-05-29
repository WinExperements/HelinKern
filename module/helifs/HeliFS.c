#include <typedefs.h>
#include <output.h>
#include <vfs.h>
#include <mm/alloc.h>
#include <debug.h>
#include <lib/string.h>
#include "HeliFS.h"

__attribute__((section(".modname"))) char *name = "helifs";
static vfs_node_t *n;

bool HeliFS_format(int partSize) {
    // Ініціалізуємо диск
    // Стовремо та запишемо супер блок
    SuperBlock *sb = (SuperBlock *)kmalloc(sizeof(SuperBlock));
    // Порахуємо потрібні данні
    uint32_t inodes = (partSize/sizeof(NodeTableEntry))/512;
    // Заповнемо його потрібними данними
    sb->magic = MAGIC;
    sb->Sectors = partSize/512;
    sb->UsedSectors = inodes;
    sb->NodeTableStart = 2;
    sb->NodeTableSize = inodes*512;
    sb->UsedNodes = 2;
    DEBUG("SB info: sb->magic = 0x%x\nsb->Sectors = %d\nsb->UsedBlocks = %d\nsb->NodeTableStart = 2\nsb->NodeTableSize = %d\n",sb->magic,sb->Sectors,sb->UsedSectors,sb->NodeTableSize);
    vfs_writeBlock(n,0,512,sb);
    // Запишемо таблицю
    NodeTableEntry *ent = (NodeTableEntry *)kmalloc(sizeof(NodeTableEntry));
    strcpy(ent->name,"/");
    ent->DataBlock = inodes+1;
    ent->Size = 0;
    // Запишемо перший  файл
    vfs_writeBlock(n,2,512,ent);
    strcpy(ent->name,"hi.txt");
    ent->Size = 3;
    ent->DataBlock = ent->DataBlock+1;
    vfs_writeBlock(n,3,512,ent);
    kfree(ent);
    // Запишемо наступні данні
    void *emptyBlock = kmalloc(512*(inodes-2));
    vfs_writeBlock(n,4,(inodes-2)*512,emptyBlock);
    kfree(emptyBlock);
    char *buff = kmalloc(512);
    strcpy(buff,"Hi!");
    vfs_writeBlock(n,inodes+2,512,buff);
    kfree(buff);
    kfree(sb);
    DEBUG_N("HeliFS formated\n");
    return true;
}

void HeliFS_mount() {
    vfs_node_t *n = vfs_find("/dev/hdap0");
    if (!n) return;
    SuperBlock *sb = kmalloc(sizeof(SuperBlock));
    vfs_readBlock(n,0,512,sb);
    DEBUG("SB info: sb->magic = 0x%x\nsb->Sectors = %d\nsb->UsedBlocks = %d\nsb->NodeTableStart = 2\nsb->NodeTableSize = %d\n",sb->magic,sb->Sectors,sb->UsedSectors,sb->NodeTableSize);
    if (sb->magic != MAGIC) {
        DEBUG("Invalid superblock: %x!\n",sb->magic);
        kfree(sb);
    }
}

static void module_main() {
    /*DEBUG_N("Formating harddrive with the FS\r\n");
    n = vfs_find("/dev/hdap0");
    if (!n) return;
    if (HeliFS_format(n->size)) {
        DEBUG_N("Formated successfuly\r\n");
    } else {
        DEBUG_N("Failed\r\n");
    }*/
    DEBUG_N("Loading FS from disk\r\n");
    HeliFS_mount();
}
