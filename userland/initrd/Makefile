all:
	cp ../init/init .
	ls | cpio -ov > ../../iso/boot/initrd.cpio
clean:
	rm -rf init
	rm -rf ../../iso/boot/initrd.cpio
