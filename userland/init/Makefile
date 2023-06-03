all:
	gcc -fno-plt -ffreestanding  -fno-stack-protector -g -m32 -nodefaultlibs -nostartfiles -I../libc -c main.c
	gcc -ffreestanding -fno-stack-protector   -g -m32 -nodefaultlibs -nostartfiles -I../libc -c test.c
	ld -melf_i386 -T../user.ld main.o ../libc/libcc.a -o init
	ld -melf_i386  -T../user.ld test.o ../libc/libcc.a -o test
	gcc -ffreestanding -fno-stack-protector   -g -m32 -nodefaultlibs -nostartfiles -I../libc -c in.c
	ld -melf_i386   -T../user.ld in.o ../libc/libcc.a -o in
	gcc -ffreestanding -fno-stack-protector  -g -m32 -nodefaultlibs -nostartfiles -I../libc -c lo.c
	ld -melf_i386  -T../user.ld lo.o ../libc/libcc.a -o lo
	cp init ../initrd
	cp test ../initrd
clean:
	rm -rf *.o init test in lo
