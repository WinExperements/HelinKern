#include <fs/parttab.h>
#include <output.h>
#include <mm/alloc.h>


typedef struct l {
	partTable_t *table;
	struct l *next;
} l_t;

l_t *tables;

void partTab_init() {
	kprintf("Partition table interface booting\n");
	tables = kmalloc(sizeof(l_t));
	memset(tables,0,sizeof(l_t));
	kprintf("Partition table interface initialized\n");
}

void partTab_register(partTable_t *lay) {
	if (tables->table == NULL) {
		tables->table = lay;
		return;
	}
	l_t *begin = tables;
	while(begin->table != NULL) {
		begin = begin->next;
	}
	// Create new entry
	l_t *newS = (l_t *)kmalloc(sizeof(l_t));
	memset(newS,0,sizeof(l_t));
	newS->table = lay;
	begin->next = newS;
}

void partTab_trigger(vfs_node_t *nod) {
	l_t *beg = tables;
	if (beg->table != NULL) {
		beg->table->search(nod);
	}
	while(beg->next != NULL) {
		beg->table->search(nod);
		beg = beg->next;
	}
}
