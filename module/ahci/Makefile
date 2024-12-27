# this file are the same for all modules excepts the objects and the module name!
OBJECTS = ahci.o ../atapi/utils.o
INC_DIR = ../../include
CC = ../../gcc-i686/bin/i686-elf-
%.o: %.c
	$(CC)gcc -m32 -march=i486 -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -fno-stack-protector -g -I$(INC_DIR) -I../ -D AS_MODULE
all: $(OBJECTS)
	$(CC)ld -melf_i386 -r -o ahci.mod $(OBJECTS)
	cp ahci.mod ../../userland/initrd/lib/modules
clean:
	@rm -rf $(OBJECTS) ahci.mod
