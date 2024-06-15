# this file are the same for all modules excepts the objects and the module name!
OBJECTS = driver.o registry.o pci.o
INC_DIR = ../../include
CC = ../../gcc-i686/bin/i686-elf-

%.o: %.c
	gcc -m32 -march=i486 -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -fno-stack-protector -I$(INC_DIR) -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o pci.mod $(OBJECTS)
	cp pci.mod ../../userland/initrd/lib/modules
clean:
	@rm -rf $(OBJECTS) pci.mod
