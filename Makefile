ARCH=x86
OBJECTS = src/arch/x86/boot.o src/arch/x86/arch.o src/kernel.o src/arch/x86/output.o src/arch/x86/io.o src/arch/x86/gdt.o src/arch/x86/x86asm.o src/dev/fb.o src/output.o font.o src/arch/x86/mmu.o src/mm/alloc.o src/isr.o src/thread.o src/lib/clist.o src/syscall.o src/dev/keyboard.o src/elf.o src/lib/string.o src/vfs.o src/rootfs.o src/dev.o src/kshell.o src/symbols.o src/module.o src/arch/x86/acpi.o src/arch/x86/smp.o src/dev/ps2mouse.o src/fs/cpio.o
CCOPTIONS =-std=gnu99 -m32 -ffreestanding -Wall -Wextra -Wno-unused-parameter -Wint-to-pointer-cast -Wsign-compare -nostdlib -march=i486 -mtune=i486 -fno-stack-protector -g -DX86
CCPATH = /home/sergij/gcc/bin/i686-elf-
%.o: %.c
	@echo [CC] $<
	$(CCPATH)gcc $(CCOPTIONS) -Iinclude -Imodule -c -o $@ $<
%.o: %.s
	@echo [ASM] $<
	@$(CCPATH)as --32 -g -o $@ $<
%.o: %.asm
	@echo [NASM] $<
	@nasm -f elf32 -F dwarf -g -o $@ $<
%.o: %.psf
	@echo [OBJCOPY] $<
	@$(CCPATH)objcopy -O elf32-i386 -B i386 -I binary $< $@
all: $(OBJECTS) $(MODULE_OBJS)
	make -C userland/libc
	make -C userland/init
	make -C module/atapi
	make -C module/mbr
	make -C module/fat32
	make -C userland/libc
	make -C userland/mount
	make -C userland/windowserver
	make -C module/pci
	make -C userland/login
	make -C userland/sh
	make -C userland/initrd
	@echo [LD] kernel.bin
	@$(CCPATH)ld -melf_i386 -T src/arch/$(ARCH)/linker.ld -Map=kernel.map -o kernel.bin $(OBJECTS) $(MODULE_OBJS)
clean:
	rm -rf $(OBJECTS) kernel.map iso/kernel userland/initrd/*.mod userland/initrd/init userland/initrd/mount userland/initrd/windowserver userland/initrd/test userland/initrd/login userland/initrd/sh
	make -C userland/init clean
	make -C module/atapi clean
	make -C module/mbr clean
	make -C module/fat32 clean
	make -C userland/libc clean
	make -C userland/mount clean
	make -C userland/windowserver clean
	make -C module/pci clean
	make -C userland/initrd clean
	make -C userland/login clean
	make -C userland/sh clean
makeiso:
	cp kernel.bin iso/kernel
	grub-mkrescue iso -o m.iso
run:
	qemu-system-i386 -cdrom m.iso -accel kvm
debug:
	qemu-system-i386 -cdrom m.iso -boot d -smp 2 -s -S &
	gdb -tui kernel.bin -x  debug.gdb
