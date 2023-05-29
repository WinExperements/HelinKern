# this file are the same for all modules excepts the objects and the module name!
OBJECTS = HeliFS.o
INC_DIR = ../../include

%.o: %.c
	gcc -m32 -g -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -I../mbr -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o HeliFS.mod $(OBJECTS)
	cp HeliFS.mod ../../iso/boot
clean:
	@rm -rf $(OBJECTS) fat32.mod
