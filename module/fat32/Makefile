# this file are the same for all modules excepts the objects and the module name!
OBJECTS = main.o
INC_DIR = ../../include
CC=../../gcc-i686/bin/i686-elf-
%.o: %.c
	$(CC)gcc -m32 -g -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -I../mbr -I../ -D AS_MODULE
all: $(OBJECTS)
	$(CC)ld -melf_i386 -r -o fat32.mod $(OBJECTS) ../../gcc-i686/lib/gcc/i686-elf/7.5.0/libgcc.a
	cp fat32.mod ../../userland/initrd/lib/modules/
clean:
	@rm -rf $(OBJECTS) fat32.mod
