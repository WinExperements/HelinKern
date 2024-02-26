#ifndef PARTLAY_H
#define PARTLAY_H

#include <typedefs.h>
#include <vfs.h>

/*
 * This header file defines kernel side interface for the partition tables
 *
 * When a new storage device is registred, the partition tables search methods are triggered for a new device
*/


typedef struct partTab {
	char *name;
	bool (*search)(vfs_node_t *disk);
} partTable_t;

void partTab_init();

void partTab_register(partTable_t *tb);
void partTab_trigger(vfs_node_t *nod);

#endif
