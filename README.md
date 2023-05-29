# HelinKern
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
# See also
See also there projects and their branches that i used to make the version:
- [SOSO OS](https://github.com/ozkl/soso)
- [ToaruOS](https://github.com/klange/toaruos/tree/toaru-1.x)
- [cuteOS](https://github.com/a-darwish/cuteOS)
- Project where you can find USB,SATA,PCI realization and many more are [here](https://github.com/pdoane/osdev)
- [ReactOS](https://github.com/reactos/reactos)
# Whats new?
In this project i make main goal to do the most portatable kernel as posible. Also with the SOSO OS allocator and VMM design i can finally implement FAT driver.
# Build
To build you need to install there software(Debian):
- `sudo apt install gcc nasm xorriso grub-common grub-pc-bin make qemu-system-i386 gdb -y`

Then do there commands:
```console
make # Build project
make makeiso # Make ISO
make run # Run via QEMU
```
