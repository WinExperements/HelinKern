#!/bin/sh
export PATH=/home/justuser/HelinKern/scripts/helinroot/bin:$PATH
i686-helin-gcc userland/newlib/init/main.c -o userland/initrd/init -DHELIN
i686-helin-gcc userland/newlib/login/main.c -o userland/initrd/login
i686-helin-gcc userland/newlib/sh/main.c -o userland/initrd/sh
i686-helin-gcc userland/newlib/tests/hddread.c -o userland/initrd/tests
