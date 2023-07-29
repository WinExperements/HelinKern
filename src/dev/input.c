#include <dev.h>
#include <dev/input.h>
#include <mm/alloc.h>
#include <lib/string.h>

int nextID = 0;

char *input_getFreeName() {
	// Allocate separe buffer with maximum lenght of 8
	char *buff = kmalloc(9); // Plus one byte for string end
	sprintf(buff,"input%d",nextID);
	nextID++;
	return buff;
}
