FIRST_OBJS =
OBJECTS = src/kernel.o src/dev/fb.o src/output.o  src/mm/alloc.o src/isr.o src/thread.o src/lib/clist.o src/syscall.o src/elf.o src/lib/string.o src/vfs.o src/rootfs.o src/dev.o src/kshell.o src/symbols.o src/module.o src/fs/cpio.o src/dev/tty.o src/dev/input.o src/dev/socket.o src/socket/unix.o src/lib/fifosize.o src/fs/partTab.o src/ipc/ipc_manager.o src/ipc/pipe.o src/ipc/shm.o module/mbr/main.o module/ext2/main.o module/iso9660/main.o src/fs/tmpfs.o src/ipc/msg.o src/dev/pts.o src/dev/audio.o
.PHONY: all
include config.mk
%.o: %.c
	@echo -ne "\r\033[K[CC] 	$<"
	@$(CCPATH)gcc $(CCOPTIONS) -Iinclude -Imodule -c -o $@ $<
%.o: %.s
	@echo -ne "\r\033[K[ASM] 		$<"
	@$(CCPATH)as $(ASPARAMS) -g -o $@ $<
%.o: %.asm
	@echo -ne "\r\033[K[NASM]	$<"  		$<
	@nasm -f elf32 -F dwarf -g -o $@ $<
%.o: %.psf
	@echo -ne "\r\033[K[OBJCOPY] 	$<"
	@$(CCPATH)objcopy  $(OBJCOPY_PARAMS) -I binary $< $@
all: $(FIRST_OBJS) $(OBJECTS) $(MODULE_OBJS)
	@echo -e "\nBuild successfull for $(ARCH)"
	make -C userland/initrd
	@echo [LD] kernel.bin
	@$(CCPATH)ld $(LDPARM) -T src/arch/$(ARCH)/$(LINKER_FILE) -o kernel.bin $(FIRST_OBJS) $(OBJECTS) $(MODULE_OBJS) $(LIBS)
clean:
	rm -rf $(OBJECTS) kernel.map iso/kernel userland/initrd/*.mod userland/initrd/init userland/initrd/bin/mount userland/initrd/bin/windowserver userland/initrd/bin/test userland/initrd/bin/login userland/bin/initrd/sh
	make -C userland/initrd clean
makeiso:
	$(MKISO_CMD)
run:
	qemu-system-$(QEMU_NAME) $(RUN_CMD)
debug:
	qemu-system-$(QEMU_NAME) $(DEBUG_CMD) &
	gdb -tui kernel.bin -x  $(DEBUG_FILE)
