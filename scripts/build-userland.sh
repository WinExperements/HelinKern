#!/bin/bash

hostDir="$(pwd)/hostbin"
pseudoGcc="/home/user/gcc-i686/bin"
sysrootPath="$(pwd)/sysroot"
helinroot="$(pwd)/helinroot"
crossgcc="$(pwd)/../gcc-i686"
scripsdir=$(pwd)
TARGET="${TARGET:=i686-helin}"
MAKE="${MAKE:=make}"


# Helper functions
function globError {
        echo "$1. Script can't continue. Exit with failure. Files will not be cleared."
        exit 1
}

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
    $MAKE
    $MAKE install
    cd ../..
    tar xf archives/autoconf-2.69.tar.gz
    mv autoconf-2.69 autoconf-host
    cd autoconf-host
    mkdir build
    cd build
    ../configure --prefix="$hostDir"
    $MAKE
    $MAKE install
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
	$MAKE -j$(nproc)
	$MAKE install
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
	$MAKE
	$MAKE install
	cd ../..
	cd autoconf
	mkdir build
	cd build
	../configure --prefix="$toA/bin"
	$MAKE
	$MAKE install
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
	$MAKE
	# nvim with nvchad just destroys our formatting. Sorry.
  	cd $toA/newlib/build
  	patch -p1 < ../../../userland/patches/newlib_v2.patch
  	cd $toA
  	echo retrying.
 	# bash build-userland.sh retry-newlib > /dev/null
 	echo Please run stage 2 using: bash build-userland.sh retry-newlib
	#echo If build failed, please remove latest symbols in $toA/newlib/build/i686-helin/newlib/libc/sys/helin/Makefile, then enter bash $0 retry-newlib
elif [ "$1" = "retry-newlib" ]; then
	toA=$(pwd)
	echo Retrying build of newlib!
	export PATH="$toA/bin/bin:$PATH"
	export PATH="$toA/../gcc-i686/bin:$PATH"
	cd newlib/build
	$MAKE -C $TARGET/newlib/libc/sys/helin || exit
	$MAKE -C $TARGET/newlib/libc/sys || exit
	$MAKE -j$(nproc) || exit
	$MAKE DESTDIR=$sysrootPath install || exit
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
	$MAKE
	$MAKE install
	cd ../../
	pwd
	cd autoconf-2.64
	mkdir build
	cd build
	bash ../configure --prefix=$instT/bin
	pwd
	$MAKE
	$MAKE install
	cd ../../
	tar xf ../archives/gcc-7.5.0.tar.gz
	mv gcc-7.5.0 src
	cd src
	patch -p1 < ../../..//userland/patches/gcc.patch
	cp $scripsdir/helin.h gcc/config
	export PATH=$instT/bin/bin:$PATH
	cd libstdc++-v3
	autoconf
	cd ..
	# We in src folder
	cd ../build
	export PATH=/home/user/gcc-i686/bin:$PATH
	cp -r $sysrootPath/usr/$TARGET $helinroot/
        mv $helinroot/$TARGET/include/sys/sdt.h $helinroot/$TARGET/include/sys/sdtt.h
	../src/configure --target=$TARGET --prefix=$helinroot --with-sysroot=$sysrootPath --enable-languages=c,c++ --with-newlib
	$MAKE all-gcc all-target-libgcc -j$(nproc)
	$MAKE all-target-libstdc++-v3 -j$(nproc)
	$MAKE install-gcc install-target-libgcc install-target-libstdc++-v3
        mv $helinroot/$TARGET/include/sys/sdtt.h $helinroot/$TARGET/include/sys/sdt.h
elif [ "$1" = "prepkernel" ]; then
	wh=$(pwd)
	mkdir ../gcc-i686
	# Extract the gcc and binutils source.
	mkdir binutils
	cd binutils
	tar xf ../archives/binutils-2.33.1.tar.gz
	./binutils-2.33.1/configure --target=i686-elf --prefix=$crossgcc || exit
	make -j$(nproc) || exit
	make install || exit
	cd ..
	rm -rf binutils
  	tar xf ../scripts/archives/gcc-7.5.0.tar.gz
  	mv gcc-7.5.0 gcc
 	cd gcc
  	mkdir build
  	cd build
  	../configure --target=i686-elf --prefix=$crossgcc || exit
  	make -j$(nproc) all-gcc || exit
  	make -j$(nproc) all-target-libgcc || exit
  	make install-gcc || exit
  	make install-target-libgcc || exit
  	cd ../..
  	rm -rf gcc
	cd ../gcc-i686
  	cp $wh/rename.sh .
  	bash rename.sh
elif [ "$1" == "build-ports" ]; then
	export PATH=$(pwd)/helinroot/bin:$PATH
        # Check if $TARGET gcc exists.
        $TARGET-gcc -v 2> /dev/null || globError "no cross compiler for $TARGET detected"
        echo Starting building base set of ports...bash,coreutils,make.
        # mkdir specific folder if required.
        mkdir -p ports
        wget -O ports/bash.tar.gz https://ftp.gnu.org/gnu/bash/bash-5.0.tar.gz
        cd ports
        tar xf bash.tar.gz
        rm -rf bash.tar.gz
        cd bash-5.0
        # Apply patch
        patch -p1 < ../../../userland/patches/bash.diff
        mkdir build
        cd build
        mkdir inst
        ../configure --prefix=$(pwd)/inst --host=i686-helin || exit
        make || exit
        make install || exit
	cp inst/bin/* ../../../../iso/bin
	cd ../../
	rm -rf bash-5.0
	wget -O coreutils.tar.gz https://ftp.gnu.org/gnu/coreutils/coreutils-8.5.tar.gz
	tar xf coreutils.tar.gz
	rm -rf coreutils.tar.gz
	cd coreutils-8.5
	patch -p1 < ../../../userland/patches/coreutils.diff
	mkdir build
	cd build
	mkdir inst
	../configure --prefix=$(pwd)/inst --host=i686-helin || exit
	make
	make install
	cp inst/bin/* ../../../../iso/bin
        cd ../../
	rm -rf coreutils-8.5
	echo "Don't worry, coreutils builded as nedded."
        wget -O make.tar.gz https://ftp.gnu.org/gnu/make/make-3.82.tar.gz
        tar xf make.tar.gz
        rm make.tar.gz
        cd make-3.82
        patch -p1 < ../../../userland/patches/make.diff
        mkdir build
        cd build
        mkdir inst
        ../configure --prefix=$(pwd)/inst --host=i686-helin || globError "Make configure error!"
        make || globError "Make build error"
        make install
        cp inst/bin/* ../../../../iso/bin
        cd ../../
        rm -rf make-3.82
        cd ..
        rm -rf ports
else
	echo Script debug! hostDir: $hostDir sysroot path: $sysrootPath root of all: $helinroot. Target: $TARGET
fi
