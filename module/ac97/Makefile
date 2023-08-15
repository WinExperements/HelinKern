# this file are the same for all modules excepts the objects and the module name!
OBJECTS =  main.o
INC_DIR = ../../include

%.o: %.c
	cc -m32 -march=i486 -mtune=i486 -c -o $@ $< -ffreestanding -fno-pic -I$(INC_DIR) -I../
all: $(OBJECTS)
	ld -melf_i386 -r -o ac97.mod $(OBJECTS)
	cp ac97.mod ../../userland/initrd
clean:
	@rm -rf $(OBJECTS) ac97.mod
