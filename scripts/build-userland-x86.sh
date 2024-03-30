#!/bin/bash
echo This scipt will automate build process for i386 architecture.
echo gcc-i686 must be in $(pwd ..). Please check if the compiler works in this path.
#bash build-userland.sh download
bash build-userland.sh prephost
bash build-userland.sh build-binutils
bash build-userland.sh build-newlib
nano newlib/build/i686-helin/newlib/libc/sys/helin/Makefile
bash build-userland.sh retry-newlib
bash build-userland.sh build-gcc
echo Building init/login/sh programs....
export PATH=$(pwd)/helinroot/bin:$PATH
i686-helin-gcc ../userland/newlib/init/main.c -o ../userland/initrd/init -DHELIN
i686-helin-gcc ../userland/newlib/login/main.c -o ../userland/initrd/login
i686-helin-gcc ../userland/newlib/sh/main.c -o ../userland/initrd/sh
echo Done.

