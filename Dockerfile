FROM ubuntu:20.04
RUN apt-get update && apt-get install -y build-essential libmpfr-dev libmpc-dev libgmp3-dev xorriso mtools grub2-common autoconf automake grub-pc-bin patch make nasm wget nano cpio
RUN useradd -ms /bin/bash user
USER user
WORKDIR /home/user
