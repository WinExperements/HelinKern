#!/bin/sh
export PATH=/home/user/HelinKern/scripts/helinroot/bin:$PATH
i686-helin-gcc userland/newlib/init/main.c -o userland/initrd/init -DHELIN -g
i686-helin-gcc userland/newlib/login/main.c -o userland/initrd/bin/login -g
i686-helin-gcc userland/newlib/sh/main.c -o userland/initrd/bin/sh -g
i686-helin-gcc userland/newlib/tests/main.c -o userland/initrd/bin/tests -g
