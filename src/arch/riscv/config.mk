ARCH = riscv
QEMU_NAME=riscv64
FIRST_OBJS += src/arch/riscv/boot.o
OBJECTS += src/arch/riscv/arch.o src/arch/riscv/mmu.o src/arch/riscv/output.o src/arch/riscv/riscv.o src/arch/riscv/opensbi.o src/arch/riscv/dev/serial.o
CCOPTIONS = -march=rv64g -mabi=lp64d -std=gnu99 -ffreestanding -nostdlib -g -fno-stack-protector -Werror -Wno-discarded-qualifiers -Wno-int-to-pointer-cast -DCONF_RING_SIZE=17
LIBS=
LDPARAM = 
CCPATH=riscv64-linux-gnu-
OBJCOPY_PARAMS = -O elf64-riscv -B riscv
RUN_CMD=-M virt -kernel kernel.bin
DEBUG_CMD=-M virt -kernel kernel.bin -s -S
