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

bool mbr_checkMBR(mbr_t *mbr) {
    return (mbr->signature[0] == 0x55) && (mbr->signature[1] == 0xAA);
}

mbr_t *mbr_getMBR();

typedef struct mbr_dev {
	int harddrive_addr;
	int part_index;
	int lba_start;
} mbr_dev_t;

#endif