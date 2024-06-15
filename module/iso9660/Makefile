# this file are the same for all modules excepts the objects and the module name!
OBJECTS =  main.o
INC_DIR = ../../include
CC = ../../gcc-i686/bin/i686-elf-
%.o: %.c
	$(CC)gcc -m32 -march=i486 -g -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -I../
all: $(OBJECTS)
	$(CC)ld -g -melf_i386 -r -o iso9660.mod $(OBJECTS)
	cp iso9660.mod ../../userland/initrd/lib/modules
clean:
	@rm -rf $(OBJECTS) iso9660.mod
