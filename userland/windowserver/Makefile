default: all
OBJECTS = main.o
%.o: %.c
	cc -m32 -nodefaultlibs -nostartfiles -c -I../libc -o $@ $<
all: $(OBJECTS)
	ld -melf_i386 -T../user.ld $(OBJECTS) ../libc/libcc.a -o windowserver
	cp windowserver ../../iso/boot 
clean:
	rm -rf $(OBJECTS) windowserver
