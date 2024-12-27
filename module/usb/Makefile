# this file are the same for all modules excepts the objects and the module name!
OBJECTS = ehci.o
INC_DIR = ../../include
CC = ../../gcc-i686/bin/i686-elf-
%.o: %.c
	$(CC)gcc -m32 -march=i486 -mtune=i486 -c -o $@ $< -ffreestanding -fno-stack-protector -fno-pic -g -I$(INC_DIR) -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o ehci.mod $(OBJECTS)
	cp ehci.mod ../../userland/initrd/lib/modules
clean:
	@rm -rf $(OBJECTS) ehci.mod
