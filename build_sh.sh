#!/bin/sh
export PATH=$(pwd)/scripts/helinroot/bin:$PATH
i686-helin-gcc userland/newlib/init/main.c -o iso/init -DHELIN -g
i686-helin-gcc userland/newlib/login/main.c -o iso/bin/login -g
i686-helin-gcc userland/newlib/sh/main.c -o iso/bin/sh -g
i686-helin-gcc userland/newlib/tests/main.c -o iso/bin/tests -g
i686-helin-gcc userland/newlib/cdboot/main.c -o userland/initrd/init -g
i686-helin-gcc userland/newlib/mount/main.c -o iso/bin/mount -g
i686-helin-gcc userland/newlib/init/poweroff.c -DINIT_REBOOT -g -o iso/bin/reboot
i686-helin-gcc userland/newlib/init/poweroff.c -DINIT_POWEROFF -g -o iso/bin/poweroff
