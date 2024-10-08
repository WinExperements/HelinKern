#!/bin/bash

hostDir="$(pwd)/hostbin"
pseudoGcc="/home/user/gcc-i686/bin"
sysrootPath="$(pwd)/sysroot"
helinroot="$(pwd)/helinroot"
TARGET="${TARGET:=i686-helin}"
echo This script will build the userland only for i386!

if [ "$1" = "download" ]; then
    mkdir archives
    cd archives
    wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz
    wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz
    wget http://ftp.gnu.org/gnu/automake/automake-1.11.tar.gz
    wget http://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.gz
    wget http://ftp.gnu.org/gnu/binutils/binutils-2.33.1.tar.gz
    wget http://ftp.gnu.org/gnu/gcc/gcc-7.5.0/gcc-7.5.0.tar.gz
    wget ftp://sourceware.org/pub/newlib/newlib-2.5.0.20171222.tar.gz
    wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.68.tar.gz
    wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.64.tar.gz
    cd ..
elif [ "$1" = "prephost" ]; then
    echo Building host specific things...
    mkdir hostbin
    tar xf archives/automake-1.15.1.tar.gz
    mv automake-1.15.1 automake-host
    cd automake-host
    mkdir build
    cd build
    ../configure --prefix="$hostDir"
    gmake
    gmake install
    cd ../..
    tar xf archives/autoconf-2.69.tar.gz
    mv autoconf-2.69 autoconf-host
    cd autoconf-host
    mkdir build
    cd build
    ../configure --prefix="$hostDir"
    gmake
    gmake install
    cd ../../
elif [ "$1" = "build-binutils" ]; then
	export PATH="$pseudoGcc:$hostDir/bin:$PATH"
	echo Building binutils for the target!
	mkdir binutils
	cd binutils
	mkdir build
	tar xf ../archives/binutils-2.33.1.tar.gz
	mv binutils-2.33.1 src
	cd src
	patch -p2 < ../../../userland/patches/binutils.patch
	cd ld
	# create the elf_i386_helin.sh and elf_x86_64_helin.sh
	cp ../../../elf_i386_helin.sh emulparams/
	cp ../../../elf_x86_64_helin.sh emulparams/
	$hostDir/bin/automake
	cd ..
	cd ../build
	../src/configure --target=$TARGET --prefix=$helinroot --with-sysroot=$sysrootPath --disable-werror
	gmake -j$(nproc)
	gmake install
elif [ "$1" = "build-newlib" ]; then
	toA=$(pwd)
	echo Building automake and autoconf for the newlib
	mkdir newlib
	cd newlib
	mkdir bin
	tar xf ../archives/newlib-2.5.0.20171222.tar.gz
	mv newlib-2.5.0.20171222 src
	mkdir build
	tar xf ../archives/automake-1.11.tar.gz
	mv automake-1.11 automake
	tar xf ../archives/autoconf-2.68.tar.gz
	mv autoconf-2.68 autoconf
	mkdir bin
	cd automake
	mkdir build
	cd build
	../configure --prefix="$toA/bin"
	gmake
	gmake install
	cd ../..
	cd autoconf
	mkdir build
	cd build
	../configure --prefix="$toA/bin"
	gmake
	gmake install
	cd ../..
	cd $toA/newlib/src
	patch -p1 < $toA/../userland/patches/newlib.patch
	export PATH="$toA/bin/bin:$PATH"
	# copy the fucking helin folder to sys
	cp -r $toA/../userland/newlib/helin newlib/libc/sys/
	cd newlib/libc/sys
	autoconf
	cd helin
	autoreconf
	cd ../../../../../build
	export PATH="$toA/../gcc-i686/bin:$PATH"
	../src/configure --target=$TARGET --prefix=/usr
	gmake
	echo If build failed, please remove latest symbols in $toA/newlib/build/i686-helin/newlib/libc/sys/helin/Makefile, then enter bash $0 retry-newlib
elif [ "$1" = "retry-newlib" ]; then
	toA=$(pwd)
	echo Retrying build of newlib!
	export PATH="$toA/bin/bin:$PATH"
	export PATH="$toA/../gcc-i686/bin:$PATH"
	cd newlib/build
	gmake -C $TARGET/newlib/libc/sys/helin || exit
	gmake -C $TARGET/newlib/libc/sys || exit
	gmake -j$(nproc) || exit
	gmake DESTDIR=$sysrootPath install || exit
elif [ "$1" = "build-gcc" ]; then
	# Here we do a GCC like stuff
	# We need to build autoconf and automake specific versions for the GCC
	mkdir gcc
	mkdir gcc/build
	mkdir gcc/bin
	cd gcc
	instT=$(pwd)
	tar xf ../archives/automake-1.11.tar.gz
	tar xf ../archives/autoconf-2.64.tar.gz
	cd automake-1.11
	mkdir build
	cd build
	../configure --prefix=$instT/bin
	gmake
	gmake install
	cd ../../
	pwd
	cd autoconf-2.64
	mkdir build
	cd build
	bash ../configure --prefix=$instT/bin
	pwd
	gmake
	gmake install
	cd ../../
	tar xf ../archives/gcc-7.5.0.tar.gz
	mv gcc-7.5.0 src
	cd src
	patch -p1 < ../../..//userland/patches/gcc.patch
	export PATH=$instT/bin/bin:$PATH
	cd libstdc++-v3
	autoconf
	cd ..
	# We in src folder
	cd ../build
	export PATH=/home/user/gcc-i686/bin:$PATH
	cp -r $sysrootPath/usr/$TARGET $helinroot/
	../src/configure --target=$TARGET --prefix=$helinroot --with-sysroot=$sysrootPath --enable-languages=c,c++ --with-newlib
	gmake all-gcc all-target-libgcc -j$(nproc)
	gmake all-target-libstdc++-v3 -j$(nproc)
	gmake install-gcc install-target-libgcc install-target-libstdc++-v3
else
	echo Script debug! hostDir: $hostDir sysroot path: $sysrootPath root of all: $helinroot. Target: $TARGET
fi
