# this file are the same for all modules excepts the objects and the module name!
OBJECTS = main.o
INC_DIR = ../../include

%.o: %.c
	cc -m32 -g -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -fno-stack-protector -I../mbr -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o ext2.mod $(OBJECTS)
	cp ext2.mod ../../userland/initrd
clean:
	@rm -rf $(OBJECTS) ext2.mod
