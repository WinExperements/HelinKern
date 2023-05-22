#ifndef STDIO_H
#define STDIO_H
#define NULL 0
typedef enum {false,true} bool;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef void FILE;
typedef unsigned int size_t;
typedef enum seek_type {
	SEEK_SET = 2,
	SEEK_END = 3,
	SEEK_CUR = 0
} seek_type_t;
void printf(const char *,...);
FILE *fopen(char *file,char *mode);
int fread(void *buff,int size,int count,FILE *file);
int fclose(FILE *file);
int fwrite(char *,int size,int how,FILE *f);
int fseek(FILE *stream,long offset,int origin);
int ftell(FILE *stream);
#endif
