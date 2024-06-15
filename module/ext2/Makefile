# this file are the same for all modules excepts the objects and the module name!
OBJECTS = main.o
INC_DIR = ../../include
CC = ../../gcc-i686/bin/i686-elf-
%.o: %.c
	$(CC)gcc -m32 -g -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -fno-stack-protector -I../mbr -I../
all: $(OBJECTS)
	$(CC)ld -melf_i386 -r -o ext2.mod $(OBJECTS)
	cp ext2.mod ../../userland/initrd/lib/modules
clean:
	@rm -rf $(OBJECTS) ext2.mod
