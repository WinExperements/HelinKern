#ifndef HELIFS_H
#define HELIFS_H
#include <typedefs.h>

#define MAGIC 0xbacf5

// Супер блок
typedef struct _SuperBlock {
    // Почнемо з інформації про носій на якому ми знаходимося
    uint32_t magic; // Ідинтифікатор ФС
    uint32_t Sectors;
    uint32_t UsedSectors;
    uint32_t NodeTableStart;
    uint32_t NodeTableSize;
    uint32_t UsedNodes;   
    char reserved[488]; 
} __attribute__((packed)) SuperBlock;

typedef struct _NodeTableEntry {
    // Елемент таблиці інойдів
    char name[24];
    int CreateTime;
    int EditTime;
    int AccessTime;
    int DataBlock;
    int Size;
} __attribute__((packed)) NodeTableEntry;

void HeliFS_mount();
bool HeliFS_format(int partSize);
void HeliFS_ls();
void HeliFS_read(NodeTableEntry *ent,int size,int off,void *buff);
void HeliFS_umount();

#endif
