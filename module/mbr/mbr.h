#ifndef MBR_H
#define MBR_H

#include <typedefs.h>

typedef struct {
	uint8_t  status;
	uint8_t  chs_first_sector[3];
	uint8_t  type;
	uint8_t  chs_last_sector[3];
	uint32_t lba_first_sector;
	uint32_t sector_count;
} partition_t;
typedef struct {
	uint8_t     boostrap[446];
	partition_t partitions[4];
	uint8_t     signature[2];
} __attribute__((packed)) mbr_t;

#define GPT_SIGNATURE 0x5452415020494645ULL // 'EFI PART'

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCRC32;
    uint32_t reserved;
    uint64_t currentLBA;
    uint64_t backupLBA;
    uint64_t firstUsableLBA;
    uint64_t lastUsableLBA;
    uint8_t diskGUID[16];
    uint64_t partitionEntryLBA;
    uint32_t numberOfPartitionEntries;
    uint32_t sizeOfPartitionEntry;
    uint32_t partitionEntryArrayCRC32;
    // Additional fields can be added as needed
} GPTHeader;

typedef struct {
    uint8_t partitionTypeGUID[16];
    uint8_t uniquePartitionGUID[16];
    uint64_t startingLBA;
    uint64_t endingLBA;
    uint64_t attributes;
    uint16_t partitionName[36];  // UTF-16 encoded partition name
    // Additional fields can be added as needed
} GPTPartitionEntry;

/*bool mbr_checkMBR(mbr_t *mbr) {
    return (mbr->signature[0] == 0x55) && (mbr->signature[1] == 0xAA);
}*/

mbr_t *mbr_getMBR();

typedef struct mbr_dev {
	int harddrive_addr;
	int part_index;
	int lba_start;
    int sectors;
} mbr_dev_t;

void mbr_init();

#endif
