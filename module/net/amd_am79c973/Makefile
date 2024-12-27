# this file are the same for all modules excepts the objects and the module name!
OBJECTS = main.o
INC_DIR = ../../../include

%.o: %.c
	gcc -m32 -march=i486 -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -fno-stack-protector -g -I$(INC_DIR) -I../../
all: $(OBJECTS)
	ld -melf_i386 -r -o amdnet.mod $(OBJECTS)
	cp amdnet.mod ../../../userland/initrd/lib/modules/
clean:
	@rm -rf $(OBJECTS) amdnet.mod ../../../userland/initrd/modules/amdnet.mod
