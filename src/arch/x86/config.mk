ARCH = x86
QEMU_NAME=i386
OBJECTS += src/arch/x86/boot.o src/arch/x86/arch.o src/arch/x86/output.o src/arch/x86/io.o src/arch/x86/gdt.o src/arch/x86/x86asm.o src/arch/x86/mmu.o src/arch/x86/acpi.o src/arch/x86/smp.o src/dev/ps2mouse.o src/dev/x86/serialdev.o src/arch/x86/dev/rtc.o src/arch/x86/firmware.o font.o src/dev/keyboard.o module/pci/driver.o module/pci/pci.o module/pci/registry.o module/net/amd_am79c973/main.o module/gpu/amdgpu/main.o module/atapi/utils.o module/atapi/atapi.o module/audio/ac97/ac97.o
CCOPTIONS =-std=gnu99 -m32 -march=i486 -mtune=i486 -ffreestanding -nostdlib -g -DX86 -fno-stack-protector -Werror -Wno-discarded-qualifiers -Wno-int-to-pointer-cast -DHWCLOCK -DCONF_RING_SIZE=17
LIBS = gcc-i686/lib/gcc/i686-elf/7.5.0/libgcc.a
CCPATH =
ASPARAMS = --32 -g
LDPARAM = -melf_i386
MKISO_CMD = cp kernel.bin iso/kernel && grub-mkrescue iso -o m.iso -V "HelinOS Live CD"
RUN_CMD = -M q35 -cdrom m.iso
DEBUG_CMD = -cdrom m.iso -s -S -boot d -M q35  -device ac97
DEBUG_FILE = debug.gdb
# Required targets.
OBJCOPY_PARAMS = -O elf32-i386 -B i386
LINKER_FILE = linker.ld
