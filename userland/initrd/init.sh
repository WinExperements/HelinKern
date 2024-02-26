#insmod /initrd/pci.mod
#insmod /initrd/atapi.mod
#insmod /initrd/mbr.mod
#insmod /initrd/ext2.mod
#insmod /initrd/atapi.mod
# Uncomment this to start graphics using the specific patch
#nano-X &
#demo-hello &
#demo-grabkey &
#lua
# Mount the devfs to /dev
mount devfs /dev /dev a
echo Abama
mount ext2 /dev/satp1 /fat a
login
