ARCH=x86
OBJECTS = src/arch/x86/boot_64.o src/arch/x86/arch.o src/kernel.o src/arch/x86/output.o src/arch/x86/gdt.o src/arch/x86/x86_64.o src/arch/x86/io.o src/dev/fb.o src/output.o font.o src/arch/x86/mmu.o src/mm/alloc.o src/isr.o src/thread.o src/lib/clist.o src/syscall.o src/dev/keyboard.o src/elf.o src/lib/string.o src/vfs.o src/rootfs.o src/dev.o src/kshell.o src/symbols.o src/module.o src/dev/ps2mouse.o src/fs/cpio.o src/dev/tty.o src/dev/input.o src/dev/socket.o src/socket/unix.o src/lib/fifosize.o src/fs/partTab.o src/dev/x86/serialdev.o src/arch/x86/dev/rtc.o src/ipc/ipc_manager.o src/ipc/pipe.o src/ipc/shm.o module/pci/driver.o module/pci/pci.o module/pci/registry.o module/mbr/main.o module/atapi/utils.o module/ext2/main.o module/iso9660/main.o module/ahci/ahci.o src/ipc/msg.o src/fs/tmpfs.o
CCOPTIONS =-std=gnu99 -ffreestanding -nostdlib -DX86 -fno-stack-protector -Werror -Wno-discarded-qualifiers -Wno-int-to-pointer-cast -DHWCLOCK -DCONF_RING_SIZE=17 -DUSE_INTERNAL_SYMS -I../experemental_os/limine-7.9.1 -g
CCPATH = 
%.o: %.c
	@echo [CC] 		$<
	@$(CCPATH)gcc $(CCOPTIONS) -Iinclude -Imodule -c -o $@ $<
%.o: %.s
	@echo [ASM] 		$<
	@$(CCPATH)as -g -o $@ $<
%.o: %.asm
	@echo [NASM]  		$<
	@nasm -f elf64 -F dwarf -o $@ $<
%.o: %.psf
	@echo [OBJCOPY] 	$<
	$(CCPATH)objcopy -O elf64-x86-64 -B i386:x86-64 -I binary $< $@
all: $(OBJECTS) $(MODULE_OBJS)
	#gmake -C module/atapi
	#gmake -C module/mbr
	#gmake -C module/ext2
	#gmake -C module/pci
	#gmake -C userland/initrd
	@echo [LD] kernel.bin
	@$(CCPATH)ld -melf_x86_64 -T src/arch/$(ARCH)/linker_64.ld -o kernel.bin $(OBJECTS) $(MODULE_OBJS) -g
clean:
	rm -rf $(OBJECTS) kernel.map iso/kernel userland/initrd/*.mod userland/initrd/init userland/initrd/bin/mount userland/initrd/bin/windowserver userland/initrd/bin/test userland/initrd/bin/login userland/bin/initrd/sh
	gmake -C module/atapi clean
	gmake -C module/mbr clean
	gmake -C module/fat32 clean
	gmake -C module/ahci clean
	gmake -C module/ext2 clean
	gmake -C module/pci clean
	gmake -C userland/initrd clean
makeiso:
	cp kernel.bin iso/kernel
	grub-mkrescue iso -o m.iso
run:
	qemu-system-i386 -M q35 -cdrom m.iso
debug:
	qemu-system-i386 -M q35 -cdrom m.iso -s -S -boot d -m 3G &
	gdb -tui kernel.bin -x  debug.gdb
