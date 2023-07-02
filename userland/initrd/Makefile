all:
	#cp ../init/init .
	find . -print | cpio -ov > ../../iso/boot/initrd.cpio
clean:
	rm -rf init
	rm -rf ../../iso/boot/initrd.cpio
