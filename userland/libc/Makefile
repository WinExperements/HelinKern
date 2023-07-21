default: all
OBJECTS = stdio.o stdlib.o start.o message.o syscall.o string.o unistd.o dirent.o wait.o sys/mman.o sys/ioctl.o
%.o: %.c
	cc -m32 -ffreestanding -fPIC -g -nostartfiles -nodefaultlibs -Wno-builtin-declaration-mismatch -fno-stack-protector -I. -c -o $@ $<
%.o: %.asm
	nasm -f elf32 -o $@ $<
all: $(OBJECTS)
	ar rc libcc.a $(OBJECTS)
clean:
	rm -rf $(OBJECTS) *.a *.so
