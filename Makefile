ARCH=x86
OBJECTS = src/arch/x86/boot.o src/arch/x86/arch.o src/kernel.o src/arch/x86/output.o src/arch/x86/io.o src/arch/x86/gdt.o src/arch/x86/x86asm.o src/dev/fb.o src/output.o font.o src/arch/x86/mmu.o src/mm/alloc.o src/isr.o src/thread.o src/lib/clist.o src/syscall.o src/dev/keyboard.o src/elf.o src/lib/string.o src/vfs.o src/rootfs.o src/dev.o src/kshell.o src/symbols.o src/module.o src/arch/x86/acpi.o src/arch/x86/smp.o src/dev/ps2mouse.o src/fs/cpio.o src/dev/tty.o src/dev/input.o src/dev/socket.o src/socket/unix.o src/lib/fifosize.o src/fs/partTab.o
CCOPTIONS =-std=gnu99 -m32 -ffreestanding -nostdlib -march=i386 -mtune=i486 -g -DX86 -fno-stack-protector -Werror -Wno-discarded-qualifiers -Wno-int-to-pointer-cast
CCPATH = 
%.o: %.c
	@echo [CC] 		$<
	@$(CCPATH)gcc $(CCOPTIONS) -Iinclude -Imodule -c -o $@ $<
%.o: %.s
	@echo [ASM] 		$<
	@$(CCPATH)as --32 -g -o $@ $<
%.o: %.asm
	@echo [NASM]  		$<
	@nasm -f elf32 -F dwarf -g -o $@ $<
%.o: %.psf
	@echo [OBJCOPY] 	$<
	@$(CCPATH)objcopy -O elf32-i386 -B i386 -I binary $< $@
all: $(OBJECTS) $(MODULE_OBJS)
	make -C module/atapi
	make -C module/mbr
	make -C module/fat32
	make -C module/pci
	make -C userland/initrd
	@echo [LD] kernel.bin
	@$(CCPATH)ld -melf_i386 -T src/arch/$(ARCH)/linker.ld -Map=kernel.map -o kernel.bin $(OBJECTS) $(MODULE_OBJS)
clean:
	rm -rf $(OBJECTS) kernel.map iso/kernel userland/initrd/*.mod userland/initrd/init userland/initrd/mount userland/initrd/windowserver userland/initrd/test userland/initrd/login userland/initrd/sh
	make -C module/atapi clean
	make -C module/mbr clean
	make -C module/fat32 clean
	make -C module/ahci clean
	make -C module/ext2 clean
	make -C module/pci clean
	make -C userland/initrd clean
makeiso:
	cp kernel.bin iso/kernel
	grub-mkrescue iso -o m.iso
run:
	qemu-system-i386 -cdrom m.iso
debug:
	qemu-system-i386 -cdrom m.iso -hda ../Стільниця/experements/disk.img -boot d -s -S -m 1G &
	gdb -tui kernel.bin -x  debug.gdb
