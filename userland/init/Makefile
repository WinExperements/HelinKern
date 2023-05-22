all:
	gcc -fno-plt -ffreestanding   -g -m32 -nodefaultlibs -nostartfiles -I../libc -c main.c
	gcc -ffreestanding  -g -m32 -nodefaultlibs -nostartfiles -I../libc -c test.c
	ld -melf_i386 -T../user.ld main.o ../libc/libcc.a -o init
	ld -melf_i386  -T../user.ld test.o ../libc/libcc.a -o test
	gcc -ffreestanding   -g -m32 -nodefaultlibs -nostartfiles -I../libc -c in.c
	ld -melf_i386   -T../user.ld in.o ../libc/libcc.a -o in
	gcc -ffreestanding  -g -m32 -nodefaultlibs -nostartfiles -I../libc -c lo.c
	ld -melf_i386  -T../user.ld lo.o ../libc/libcc.a -o lo
	cp init ../../iso/boot
	cp test ../../iso/boot/
	cp lo ../../iso/boot/lo
clean:
	rm -rf *.o init test in lo
