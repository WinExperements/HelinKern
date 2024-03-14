# Building userland.

Yes, the scripts finally writen.

To start building you need to download `gcc 7.5.0` from `OSDEV cross compiller page`(you can use another cross compiller or use your own) and installl everything to run i686-elf-gcc and i686-elf-ar, then make symlinks of all files to create copy of there files with start  prefix of i686-helin-

## begin of process

### NOTE: The gcc-i686 must be located at parent directory of scripts folder.

Simply run there command in terminal:
```console
bash build-userland.sh download
bash build-userland.sh prephost
bash build-userland.sh build-binutils
bash build-userland.sh build-newlib

# At this moment remove all newlines at end of the file(where the objects are counted and the all: pointed)
# then run
bash build-userland.sh retry-newlib
bash build-userland.sh build-gcc
# At the end the gcc binaries are be located at helinroot/bin. 

```

## Some info
The additional files places here are required to build binutils due to broken patch file placed at root/userland/patches.

## For x86
If you want to build all userland including the userland programs for x86, use this commands:
```console
bash build-userland-x86.sh
<when nano will be openned, remove all newlines and save-exit.
make -C ../ all makeiso
```
Now the userland is builded and can be used.
