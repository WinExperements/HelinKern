#!/bin/bash
cd bin
for file in *elf*; do
	newname=$(echo "$file" | sed 's/elf/helin/')
	ln -vs $file $newname
done
