all:
	gcc -m32 -nodefaultlibs -nostartfiles -I../libc -c main.c -g
	ld -g -melf_i386 -T../user.ld main.o ../libc/libcc.a -o mount
	cp mount ../../iso/boot
clean:
	rm -rf *.o mount
