#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <sys/stat.h>
void generateEmpty() {
  FILE *out = fopen("../userland/ksymbols.h","w");
	if (!out) {
		perror("failed to open output file");
    return;
	}
	fprintf(out,"#pragma once\r\ntypedef struct ksm {char *name; int val;} kernel_sym_t;\r\n");
  fprintf(out,"#define KERN_SYM_COUNT 1\r\n");
	fprintf(out,"kernel_sym_t kernel_syms[KERN_SYM_COUNT] = {\r\n");
  fprintf(out,"   {\"\",0}\r\n};");
  fclose(out);
  printf("Successfully generated first time empty ksymbols.h\r\n");
}
int main() {
	int knfd = open("../kernel.bin",O_RDONLY);
	if (knfd < 0) {
		perror("Failed to open kernel image, fallback to creating empty ksymbols.h for the first time build");
    generateEmpty();
		return 1;
	}
	if (elf_version(EV_CURRENT) == EV_NONE) {
		printf("libelf init error: %s\r\n",elf_errmsg(-1));
		return 1;
	}
	// retrive file size.
	Elf *elf = elf_begin(knfd,ELF_C_READ,NULL);
	if (!elf) {
		printf("elf_begin error: %s\r\n",elf_errmsg(-1));
		return 1;
	}
	GElf_Ehdr ehdr;
	if (!gelf_getehdr(elf,&ehdr)) {
		printf("ELF ehdr error\r\n");
		close(knfd);
		return 1;
	}
	Elf_Scn *scn = NULL;
	FILE *out = fopen("ksymbols.h","w");
	if (!out) {
		perror("failed to open output file");
    return 1;
	}
	fprintf(out,"#pragma once\r\ntypedef struct ksm {char *name; int val;} kernel_sym_t;\r\n");
	while((scn = elf_nextscn(elf,scn)) != NULL) {
		GElf_Shdr shdr;
		if (!gelf_getshdr(scn,&shdr)) {
			printf("shdr get error\r\n");
			return 1;
		}
		if (shdr.sh_type == SHT_SYMTAB) {
			Elf_Data *data = NULL;
			data = elf_getdata(scn,data);
			if (!data) {
				printf("data get err\r\n");
				return 1;
			}
			Elf_Scn *strtab_scn = elf_getscn(elf,shdr.sh_link);
			if (!strtab_scn) {
				printf("Strtab get err\r\n");
				return 1;
			}
			Elf_Data *strtab_data = elf_getdata(strtab_scn,NULL);
			size_t num_symbols = shdr.sh_size / shdr.sh_entsize;
			fprintf(out,"#define KERN_SYM_COUNT %lu\r\n",num_symbols);
			fprintf(out,"kernel_sym_t kernel_syms[KERN_SYM_COUNT] = {\r\n");
			for (size_t i = 0; i < num_symbols; i++) {
				GElf_Sym sym;
				gelf_getsym(data,i,&sym);
				const char *name = elf_strptr(elf,shdr.sh_link,sym.st_name);
				fprintf(out,"	{\"%s\",0x%lx},\r\n",name,(unsigned long)sym.st_value);
				//printf("Sym: %s, val: 0x%x\r\n",name,(unsigned long)sym.st_value);
			}
		}
	}
	elf_end(elf);
	fprintf(out,"\r\n};");
	fclose(out);
	close(knfd);
	return 0;
}
