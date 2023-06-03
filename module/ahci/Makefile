# this file are the same for all modules excepts the objects and the module name!
OBJECTS = ahci.o
INC_DIR = ../../include

%.o: %.c
	gcc -m32 -march=i486 -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -g -I$(INC_DIR) -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o ahci.mod $(OBJECTS)
	cp ahci.mod ../../userland/initrd/
clean:
	@rm -rf $(OBJECTS) ahci.mod
