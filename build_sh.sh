#!/bin/sh
export PATH=$(pwd)/scripts/helinroot/bin:$PATH
x86_64-helin-gcc userland/newlib/init/main.c -o userland/initrd/init -DHELIN -g
x86_64-helin-gcc userland/newlib/login/main.c -o userland/initrd/bin/login -g
x86_64-helin-gcc userland/newlib/sh/main.c -o userland/initrd/bin/sh -g
x86_64-helin-gcc userland/newlib/tests/main.c -o userland/initrd/bin/tests -g
x86_64-helin-gcc userland/newlib/cdboot/main.c -o userland/initrd/bin/cdboot -g
