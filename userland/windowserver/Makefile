default: all
OBJECTS = main.o
%.o: %.c
	cc -std=gnu99 -m32 -ffreestanding -Wall -Wextra -Wno-unused-parameter -Wint-to-pointer-cast -Wsign-compare -nostdlib -march=i486 -mtune=i486 -fno-stack-protector  -c -g -I../libc -o $@ $<
all: $(OBJECTS)
	ld -melf_i386 -T../user.ld $(OBJECTS) ../libc/libcc.a -o windowserver
	cp windowserver ../userland
clean:
	rm -rf $(OBJECTS) windowserver
