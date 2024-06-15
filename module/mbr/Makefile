# this file are the same for all modules excepts the objects and the module name!
OBJECTS =  main.o
INC_DIR = ../../include
CC = ../../gcc-i686/bin/i686-elf-
%.o: %.c
	$(CC)gcc -m32 -march=i486 -g -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -I../
all: $(OBJECTS)
	$(CC)ld -g -melf_i386 -r -o mbr.mod $(OBJECTS)
	cp mbr.mod ../../userland/initrd/lib/modules
clean:
	@rm -rf $(OBJECTS) mbr.mod
