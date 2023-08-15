# HelinKern

![screenshot](res/screenshot.png)

just new implementation of my old osdev project. Many components are based on open source implementation, check the files for more information.
# Sources
The kernel uses source code and/or the design from there projects:
- SOSO OS
- ToaruOS
- cuteOS
- JamesMolloy osdev
- BrokenThorn osdev
- My old osdev project
- At some point ReactOS
- And many many others projects(all there repos just named as osdev)
- OSDEV wiki
- GRUB(module loading,AHCI and many many other thinks)
# See also
See also there projects and their branches that i used to make the version:
- [SOSO OS](https://github.com/ozkl/soso)
- [ToaruOS](https://github.com/klange/toaruos/tree/toaru-1.x)
- [cuteOS](https://github.com/a-darwish/cuteOS)
- Project where you can find USB,SATA,PCI realization and many more are [here](https://github.com/pdoane/osdev)
- [ReactOS](https://github.com/reactos/reactos)
- [Used GRUB](https://github.com/rhboot/grub2)
# Build
To build you need to install there software(Debian):
- `sudo apt install gcc nasm xorriso grub-common grub-pc-bin make qemu-system-i386 gdb -y`

Then change cross-compiler path in Makefile(you can leave it empty, so it can be compiled with system tools)

Then do there commands:
```console
make # Build project
make makeiso # Make ISO
make run # Run via QEMU
```

# System Requirements
- 486+ CPU for base system, Pentium Pro+ for newlib applications
- 20M of memory to boot base system
- If the system doesn't boot in graphical mode, then disable UI mode [here](src/arch/x86/boot.s), and set `dontFB` [here](src/arch/x86/arch.c) to `true` and the system must boot in VGA text mode

# Userland
The system support newlib applicaitions, but the building process will be documented later, but if you want to do it manually you can apply GCC and binutils patchs and build it using OSDEV OS-specific toolchain build tutorial

