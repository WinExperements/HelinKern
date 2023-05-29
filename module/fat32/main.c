#include <output.h>
#include <dev.h>
#include <mm/alloc.h>
#include <vfs.h>
#include <mbr.h>
#include <lib/string.h>
#include "fat32.h"
#define SECTOR_SIZE             512
#define MAX_PATH_SIZE           256
#define MAX_FILE_HANDLES        10
#define ROOT_DIRECTORY_HANDLE   -1
#define FAT_CACHE_SIZE          5

// this filesystem can be mounted only on ONE drive!
char modname[] __attribute__((section(".modname"))) = "fat32";
static vfs_node_t *disk;
static vfs_fs_t *fat32;

typedef struct 
{
    // extended boot record
    uint8_t DriveNumber;
    uint8_t _Reserved;
    uint8_t Signature;
    uint32_t VolumeId;          // serial number, value doesn't matter
    uint8_t VolumeLabel[11];    // 11 bytes, padded with spaces
    uint8_t SystemId[8];
} __attribute__((packed)) FAT_ExtendedBootRecord;

typedef struct 
{
    uint32_t SectorsPerFat;
    uint16_t Flags;
    uint16_t FatVersion;
    uint32_t RootDirectoryCluster;
    uint16_t FSInfoSector;
    uint16_t BackupBootSector;
    uint8_t _Reserved[12];
    FAT_ExtendedBootRecord EBR;

} __attribute((packed)) FAT32_ExtendedBootRecord;

typedef struct 
{
    uint8_t BootJumpInstruction[3];
    uint8_t OemIdentifier[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FatCount;
    uint16_t DirEntryCount;
    uint16_t TotalSectors;
    uint8_t MediaDescriptorType;
    uint16_t SectorsPerFat;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t LargeSectorCount;

    union {
        FAT_ExtendedBootRecord EBR1216;
        FAT32_ExtendedBootRecord EBR32;
    };

    // ... we don't care about code ...

} __attribute__((packed)) FAT_BootSector;

typedef struct {
    uint8_t Order;
    int16_t Chars[13];
} FAT_LFNBlock;

typedef struct 
{
    int Handle;
    bool IsDirectory;
    uint32_t Position;
    uint32_t Size;
} FAT_File;

typedef struct
{
    uint8_t Buffer[512];
    FAT_File Public;
    bool Opened;
    uint32_t FirstCluster;
    uint32_t CurrentCluster;
    uint32_t CurrentSectorInCluster;

} FAT_FileData;

typedef struct {
     union
    {
        FAT_BootSector BootSector;
        uint8_t BootSectorBytes[512];
    } BS;
    FAT_FileData RootDirectory;
    FAT_FileData OpenedFiles[MAX_FILE_HANDLES];
    uint8_t FatCache[5 * 512];
    uint32_t LFNCount;
    uint32_t FatCachePosition;
} FAT_Data;

typedef struct 
{
    uint8_t Name[11];
    uint8_t Attributes;
    uint8_t _Reserved;
    uint8_t CreatedTimeTenths;
    uint16_t CreatedTime;
    uint16_t CreatedDate;
    uint16_t AccessedDate;
    uint16_t FirstClusterHigh;
    uint16_t ModifiedTime;
    uint16_t ModifiedDate;
    uint16_t FirstClusterLow;
    uint32_t Size;
} __attribute__((packed)) FAT_DirectoryEntry;


enum FAT_Attributes
{
    FAT_ATTRIBUTE_READ_ONLY         = 0x01,
    FAT_ATTRIBUTE_HIDDEN            = 0x02,
    FAT_ATTRIBUTE_SYSTEM            = 0x04,
    FAT_ATTRIBUTE_VOLUME_ID         = 0x08,
    FAT_ATTRIBUTE_DIRECTORY         = 0x10,
    FAT_ATTRIBUTE_ARCHIVE           = 0x20,
    FAT_ATTRIBUTE_LFN               = FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID
};

// define not implemented or unused functions here
static struct dirent *fat32_readdir(vfs_node_t *node,uint32_t index);
static FAT_Data *g_Data = NULL;
static uint32_t g_DataSectionLba;
static uint8_t g_FatType;
static uint32_t g_TotalSectors;
static uint32_t g_SectorsPerFat;


static uint32_t FAT_ClusterToLba(uint32_t cluster)
{
    return g_DataSectionLba + (cluster - 2) * g_Data->BS.BootSector.SectorsPerCluster;
}

static void FAT_ReadFat(vfs_node_t* disk, size_t lbaIndex)
{
    vfs_readBlock(disk, g_Data->BS.BootSector.ReservedSectors + lbaIndex, 5*512, g_Data->FatCache);
}

void FAT_Detect()
{
    uint32_t dataClusters = (g_TotalSectors - g_DataSectionLba) / g_Data->BS.BootSector.SectorsPerCluster;
    if (dataClusters < 0xFF5) 
        g_FatType = 12;
    else if (g_Data->BS.BootSector.SectorsPerFat != 0)
        g_FatType = 16;
    else g_FatType = 32;
}

static bool fat32_mount(vfs_node_t *to,vfs_node_t *root,void *params) {
	disk = to;
	// Stage1: read the BPB(BIOS Boot Block) from disk then parse it
    g_Data = kmalloc(sizeof(FAT_Data));
    memset(g_Data,0,sizeof(FAT_Data));
    vfs_readBlock(disk,0,512,g_Data);
    g_TotalSectors = g_Data->BS.BootSector.TotalSectors;
    if (g_TotalSectors == 0) {          // fat32
        g_TotalSectors = g_Data->BS.BootSector.LargeSectorCount;
    }
    bool isFat32 = false;
    g_SectorsPerFat = g_Data->BS.BootSector.SectorsPerFat;
    if (g_SectorsPerFat == 0) {         // fat32
        isFat32 = true;
        g_SectorsPerFat = g_Data->BS.BootSector.EBR32.SectorsPerFat;
    }
    kprintf("Is FAT32: %s\r\n",(isFat32 ? "Yes" : "No"));
    uint32_t rootDirLba;
    uint32_t rootDirSize;
    if (isFat32) {
        g_DataSectionLba = g_Data->BS.BootSector.ReservedSectors + g_SectorsPerFat * g_Data->BS.BootSector.FatCount;
        rootDirLba = FAT_ClusterToLba( g_Data->BS.BootSector.EBR32.RootDirectoryCluster);
        rootDirSize = 0;
    }
    else {
        rootDirLba = g_Data->BS.BootSector.ReservedSectors + g_SectorsPerFat * g_Data->BS.BootSector.FatCount;
        rootDirSize = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
        uint32_t rootDirSectors = (rootDirSize + g_Data->BS.BootSector.BytesPerSector - 1) / g_Data->BS.BootSector.BytesPerSector;
        g_DataSectionLba = rootDirLba + rootDirSectors;
    }
    // Read root directory
    vfs_readBlock(disk,rootDirLba,512,g_Data->RootDirectory.Buffer);
    FAT_Detect();
    g_Data->RootDirectory.Public.Handle = ROOT_DIRECTORY_HANDLE;
    g_Data->RootDirectory.Public.IsDirectory = true;
    g_Data->RootDirectory.Public.Position = 0;
    g_Data->RootDirectory.Public.Size = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
    g_Data->RootDirectory.Opened = true;
    g_Data->RootDirectory.FirstCluster = rootDirLba;
    g_Data->RootDirectory.CurrentCluster = rootDirLba;
    g_Data->RootDirectory.CurrentSectorInCluster = 0;
    for (int i = 0; i < MAX_FILE_HANDLES; i++)
        g_Data->OpenedFiles[i].Opened = false;
    g_Data->LFNCount = 0;
    root->fs = fat32;
    kprintf("FAT32 mounted\r\n");
	return true;
}

uint32_t FAT_NextCluster(vfs_node_t* disk, uint32_t currentCluster)
{    
    // Determine the byte offset of the entry we need to read
    uint32_t fatIndex;
    if (g_FatType == 12) {
        fatIndex = currentCluster * 3 / 2;
    }
    else if (g_FatType == 16) {
        fatIndex = currentCluster * 2;
    }
    else /*if (g_FatType == 32)*/ {
        fatIndex = currentCluster * 4;
    }

    // Make sure cache has the right number
    uint32_t fatIndexSector = fatIndex / 512;
    if (fatIndexSector < g_Data->FatCachePosition 
        || fatIndexSector >= g_Data->FatCachePosition + 5)
    {
        FAT_ReadFat(disk, fatIndexSector);
        g_Data->FatCachePosition = fatIndexSector;
    }

    fatIndex -= (g_Data->FatCachePosition * 512);

    uint32_t nextCluster;
    if (g_FatType == 12) {
        if (currentCluster % 2 == 0)
            nextCluster = (*(uint16_t*)(g_Data->FatCache + fatIndex)) & 0x0FFF;
        else
            nextCluster = (*(uint16_t*)(g_Data->FatCache + fatIndex)) >> 4;
        
        if (nextCluster >= 0xFF8) {
            nextCluster |= 0xFFFFF000;
        }
    }
    else if (g_FatType == 16) {
        nextCluster = *(uint16_t*)(g_Data->FatCache + fatIndex);
        if (nextCluster >= 0xFFF8) {
            nextCluster |= 0xFFFF0000;
        }
    }
    else /*if (g_FatType == 32)*/ {
        nextCluster = *(uint32_t*)(g_Data->FatCache + fatIndex);
    }

    return nextCluster;
}


uint32_t FAT_Read(vfs_node_t *disk, FAT_File* file, uint32_t byteCount, void* dataOut)
{
    // get file data
    FAT_FileData* fd = (file->Handle == ROOT_DIRECTORY_HANDLE) 
        ? &g_Data->RootDirectory 
        : &g_Data->OpenedFiles[file->Handle];

    uint8_t* u8DataOut = (uint8_t*)dataOut;

    // don't read past the end of the file
    if (!fd->Public.IsDirectory || (fd->Public.IsDirectory && fd->Public.Size != 0))
        byteCount = min(byteCount, fd->Public.Size - fd->Public.Position);

    while (byteCount > 0)
    {
        uint32_t leftInBuffer = SECTOR_SIZE - (fd->Public.Position % SECTOR_SIZE);
        uint32_t take = min(byteCount, leftInBuffer);

        memcpy(u8DataOut, fd->Buffer + fd->Public.Position % SECTOR_SIZE, take);
        u8DataOut += take;
        fd->Public.Position += take;
        byteCount -= take;

        // printf("leftInBuffer=%lu take=%lu\r\n", leftInBuffer, take);
        // See if we need to read more data
        if (leftInBuffer == take)
        {
            // Special handling for root directory
            if (fd->Public.Handle == ROOT_DIRECTORY_HANDLE)
            {
                ++fd->CurrentCluster;

                // read next sector
                vfs_readBlock(disk, fd->CurrentCluster, 512, fd->Buffer);
                // calculate next cluster & sector to read
                if (++fd->CurrentSectorInCluster >= g_Data->BS.BootSector.SectorsPerCluster)
                {
                    fd->CurrentSectorInCluster = 0;
                    fd->CurrentCluster = FAT_NextCluster(disk, fd->CurrentCluster);
                }

                if (fd->CurrentCluster >= 0xFFFFFFF8)
                {
                    // Mark end of file
                    fd->Public.Size = fd->Public.Position;
                    break;
                }

                // read next sector
                vfs_readBlock(disk, FAT_ClusterToLba(fd->CurrentCluster) + fd->CurrentSectorInCluster, 512, fd->Buffer);
        }}
    }

    return u8DataOut - (uint8_t*)dataOut;
}


bool FAT_ReadEntry(vfs_node_t* disk, FAT_File* file, FAT_DirectoryEntry* dirEntry)
{
    return FAT_Read(disk, file, sizeof(FAT_DirectoryEntry), dirEntry) == sizeof(FAT_DirectoryEntry);
}


FAT_File* FAT_OpenEntry(vfs_node_t* disk, FAT_DirectoryEntry* entry)
{
    // find empty handle
    int handle = -1;
    for (int i = 0; i < MAX_FILE_HANDLES && handle < 0; i++)
    {
        if (!g_Data->OpenedFiles[i].Opened)
            handle = i;
    }

    // out of handles
    if (handle < 0)
    {
        kprintf("FAT: out of file handles\r\n");
        return false;
    }

    // setup vars
    FAT_FileData* fd = &g_Data->OpenedFiles[handle];
    fd->Public.Handle = handle;
    fd->Public.IsDirectory = (entry->Attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    fd->Public.Position = 0;
    fd->Public.Size = entry->Size;
    fd->FirstCluster = entry->FirstClusterLow + ((uint32_t)entry->FirstClusterHigh << 16);
    fd->CurrentCluster = fd->FirstCluster;
    fd->CurrentSectorInCluster = 0;
    vfs_readBlock(disk,FAT_ClusterToLba(fd->CurrentCluster),512,fd->Buffer);

    fd->Opened = true;
    return &fd->Public;
}

static struct dirent root_dirent;

static struct dirent *fat32_readdir(vfs_node_t *node,uint32_t index) {
    // Що тут робити?
    // Ми самі логічні, а тому код буде лайном
    FAT_File *current = &g_Data->RootDirectory.Public;
    FAT_DirectoryEntry entry;
    if (!FAT_ReadEntry(disk,current,&entry)) {
        g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
        g_Data->RootDirectory.Public.Position = 0;
        g_Data->RootDirectory.CurrentSectorInCluster = 0;
    }
    // Зчитаємо наш індекс
    for (int i = 0; i < index; i++) {
        // Капець як логічно, правда?
        if (!FAT_ReadEntry(disk,current,&entry)) {
            g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
            g_Data->RootDirectory.Public.Position = 0;
            g_Data->RootDirectory.CurrentSectorInCluster = 0;
            return NULL;
        }
         if ((entry.Size == 0) && (entry.Attributes != FAT_ATTRIBUTE_DIRECTORY)) {
                g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
                g_Data->RootDirectory.Public.Position = 0;
                g_Data->RootDirectory.CurrentSectorInCluster = 0;
            	return NULL;
	    }
	
     }
     g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
     g_Data->RootDirectory.Public.Position = 0;
     g_Data->RootDirectory.CurrentSectorInCluster = 0;
    // Повернемо об'єкт директорії
    strcpy(root_dirent.name,entry.Name);
	return &root_dirent;
}

void FAT_GetShortName(const char* name, char shortName[12])
{
    // convert from name to fat name
    memset(shortName, ' ', 12);
    shortName[11] = '\0';

    const char* ext = strchr(name, '.');
    if (ext == NULL)
        ext = name + 11;

    for (int i = 0; i < 8 && name[i] && name + i < ext; i++)
        shortName[i] = toupper(name[i]);

    if (ext != name + 11)
    {
        for (int i = 0; i < 3 && ext[i + 1]; i++)
            shortName[i + 8] = toupper(ext[i + 1]);
    }
}

static void fat32_read(vfs_node_t *w,uint64_t off,uint64_t size,void *buff) {
    FAT_File *entry = w->priv_data;
    FAT_Read(disk,entry,size,buff);
    
}
static vfs_node_t *fat32_finddir(vfs_node_t *root,char *name) {
    // Зчитаємо всі файли та знайдемо тебе!
    char shortName[12];
    FAT_GetShortName(name, shortName);
    FAT_File *current = &g_Data->RootDirectory.Public;
    FAT_DirectoryEntry entry;
    while(FAT_ReadEntry(disk,current,&entry)) {
        if (entry.Size == 0) {
           continue;
        }
         if (memcmp(shortName, entry.Name, 11) == 0)
        {
            // Стоврюємо об'єкт ФС для цього файлу
            vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
            memset(node,0,sizeof(vfs_node_t));
            strcpy(node->name,shortName);
            node->fs = fat32;
            node->size = entry.Size;
            FAT_File *copy = FAT_OpenEntry(disk,&entry);
            node->priv_data = copy;
            // reset pos
            g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
            g_Data->RootDirectory.Public.Position = 0;
            g_Data->RootDirectory.CurrentSectorInCluster = 0;
            return node;
        }
    }
    g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
    g_Data->RootDirectory.Public.Position = 0;
    g_Data->RootDirectory.CurrentSectorInCluster = 0;
    return NULL;
}

static void fat32_close(vfs_node_t *node) {
    /*FAT_File *file = (FAT_File *)node->priv_data;
    if (file->Handle == ROOT_DIRECTORY_HANDLE)
    {
        file->Position = 0;
        g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
    }
    else
    {
        g_Data->OpenedFiles[file->Handle].Opened = false;
    }*/
    kfree(node);
}

static void module_main() {
	// register FS
	fat32 = kmalloc(sizeof(vfs_fs_t));
    memset(fat32,0,sizeof(vfs_fs_t));
	fat32->fs_name = "fat32";
	fat32->mount = fat32_mount;
    fat32->readdir = fat32_readdir;
    fat32->read = fat32_read;
    fat32->finddir = fat32_finddir;
    fat32->close = fat32_close;
	vfs_addFS(fat32);
	kprintf("FAT32 added\n");
}
