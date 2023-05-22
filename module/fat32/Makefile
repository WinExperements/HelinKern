# this file are the same for all modules excepts the objects and the module name!
OBJECTS = main.o
INC_DIR = ../../include

%.o: %.c
	gcc -m32 -g -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -I../mbr -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o fat32.mod $(OBJECTS)
	cp fat32.mod ../../iso/boot
clean:
	@rm -rf $(OBJECTS) fat32.mod
