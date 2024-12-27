echo Welcome to Bash based HelinKern test script.
echo This script will build the test program
echo within the NASM test application and 
echo link it using GNU Binutils.
echo Report all bugs to:
echo  https://github.com/WinExperements/HelinKern
export PATH=/bin:/mnt/bin
# This is a multi-architectural bash script
echo Detecting Architecture.
if [ "$MACHTYPE" = "i686-pc-helin" ]; then
  # This is 32 bit Architecture, the files placed in different place.
  nasm -f elf32 /boot.asm -o /tmp/a 
elif [ "$MACHTYPE" = "x86_64-pc-helin" ]; then
  echo Building 64 bit application
  nasm -f elf64 /mnt/testapp.asm -o /tmp/a
  ld /tmp/a -o /tmp/b
  /tmp/b
  echo Builded.
elif [ "$MACHTYPE" = "x86_64-pc-linux-gnu" ]; then
  echo TEst
else
  echo "Unknwon Architecture $MACHTYPE!"
  exit 1
fi 
# Now main part of code
echo Linking
ld /tmp/a -o /tmp/b 
/tmp/b
echo OK?
