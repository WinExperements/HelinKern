all:
	gcc -std=gnu99 -m32 -ffreestanding -Wall -Wextra -Wno-unused-parameter -Wint-to-pointer-cast -Wsign-compare -nostdlib -march=i486 -mtune=i486 -fno-stack-protector -I../libc -c main.c -g
	ld -g -melf_i386 -T../user.ld main.o ../libc/libcc.a -o mount
	cp mount ../userland
clean:
	rm -rf *.o mount
