/*
 * storage.h
 *
 * Created: 24.09.2020 17:22:04
 * Author : Micha� Granda
 */ 


#ifndef STORAGE_H_
#define STORAGE_H_

#include <typedefs.h>
#include "../slimfaterr.h"

#define SECTOR_SIZE 512

typedef struct {
	/* Storage media object */
	void* disk;
	/* Storage media status */
	uint8_t status;
	/* Buffered operations */
	uint32_t sector;
	uint8_t* buffer;
	/* Function pointers to access raw data */
	uint8_t(*read_sector)(void*, const uint32_t, uint8_t*);
	uint8_t(*write_sector)(void*, const uint32_t, const uint8_t*);
} fs_storage_device;

#define GET_DEV_HANDLE(buff, dev, read, write) {.disk = dev, .status = 0, .buffer = buff,  .read_sector = read, .write_sector = write }

fs_error find_partition(fs_storage_device* device, const uint8_t partition_number, uint32_t* sector);
fs_error read_buffered_sector(fs_storage_device* device, const uint32_t sector);
fs_error write_buffered_sector(fs_storage_device* device, const uint32_t sector);
uint8_t* get_raw_buffer(fs_storage_device* device);
void set_pending_write(fs_storage_device* device);

#endif /* STORAGE_H_ */
