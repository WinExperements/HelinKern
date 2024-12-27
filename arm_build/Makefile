ARCH= armv7-a
OBJECTS = ../src/arch/armv7-a/boot_msm8909.o ../src/arch/armv7-a/arch.o ../src/arch/armv7-a/output.o ../src/kernel.o ../src/dev/fb.o ../src/output.o font.o ../src/arch/armv7-a/mmu.o ../src/mm/alloc.o ../src/isr.o ../src/thread.o ../src/lib/clist.o ../src/syscall.o ../src/elf.o ../src/lib/string.o ../src/vfs.o ../src/rootfs.o ../src/dev.o ../src/kshell.o ../src/symbols.o ../src/module.o ../src/fs/cpio.o ../src/dev/tty.o ../src/dev/input.o ../src/dev/socket.o ../src/socket/unix.o ../src/lib/fifosize.o ../src/fs/partTab.o ../src/fs/tmpfs.o ../src/ipc/ipc_manager.o ../src/ipc/msg.o ../src/ipc/shm.o ../src/ipc/pipe.o ../src/dev/pts.o
CCOPTIONS =-std=gnu99 -ffreestanding -Wall -Wextra -Wno-unused-parameter -Wint-to-pointer-cast -Wsign-compare -nostdlib -fno-stack-protector -g -march=armv7-a -DHELINBOOT_USED -Werror -Wno-discarded-qualifiers -Wno-int-to-pointer-cast -DCONF_RING_SIZE=11 -Wno-error=sign-compare -Wno-error=unused-label -Wno-error=type-limits -Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-but-set-variable
CCPATH = arm-none-eabi-
%.o: %.c
	@echo [CC] $<
	@$(CCPATH)gcc $(CCOPTIONS) -I../include -I../module -DQEMUBOARD -c -o $@ $<
%.o: %.s
	@echo [ASM] $<
	@$(CCPATH)as -g -o $@ $<
%.o: %.asm
	@echo [NASM] $<
	@nasm -f elf32 -F dwarf -g -o $@ $<
%.o: %.psf
	@echo [OBJCOPY] $<
	@$(CCPATH)objcopy -O elf32-littlearm -B arm -I binary $< $@
all: $(OBJECTS) $(MODULE_OBJS)
	@echo [LD] kernel.bin
	$(CCPATH)gcc -T ../src/arch/$(ARCH)/linker.ld -o kernel_arm.bin $(OBJECTS) $(MODULE_OBJS) -lgcc -nostartfiles -nodefaultlibs
clean:
	rm -rf $(OBJECTS) kernel.map iso/kernel iso/boot/*.mod iso/boot/init iso/boot/lo iso/boot/mount iso/boot/test
	make -C userland/init clean
	make -C module/atapi clean
	make -C module/mbr clean
	make -C module/fat32 clean
	make -C userland/libc clean
	make -C userland/mount clean
	make -C userland/windowserver clean
	make -C module/pci clean
	make -C userland/initrd clean
makeiso:
	cp kernel.bin iso/kernel
	grub-mkrescue iso -o m.iso
run:
	qemu-system-i386 -cdrom m.iso -accel kvm
debug:
	qemu-system-i386 -cdrom m.iso -boot d -smp 2 -s -S &
	gdb -tui kernel.bin -x  debug.gdb
