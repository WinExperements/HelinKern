#include <lib/string.h>
#include <mm/alloc.h>
#include <output.h>
static char *oldword = 0;
void memcpy(void *vd, const void *vs, unsigned length) {
	char *d = vd;
	const char *s = vs;
	while(length) {
		*d = *s;
		d++;
		s++;
		length--;
	}
}
bool strcmp(char *s1,char *s2) {
	int len1 = strlen(s1);
	int len2 = strlen(s2);
	int checkedLen = 0;
	if (len1 > len2 || len1 < len2) {
		return false;
	}
	/* If length are equal then check the symbols */
	for (int i = 0; i < len1; i++) {
		if (s1[i] == s2[i]) {
			checkedLen++;
		}
	}
	if (checkedLen == len1) {
		return true;
	} else {
		return false;
	}
}
bool strcpy(char *dest,const char *src) {
     int len = 0;
	 do
    {
      *dest++ = *src++;
      len++;
    }
    while (*src != 0);
    *dest = '\0';
	return true;
}
const char *strchr(const char *s, char ch)
{
	while(*s) {
		if(*s == ch)
			return s;
		s++;
	}
	return 0;
}

char *strtok(char *s, const char *delim)
{
	char *word;

	if(!s)
		s = oldword;

	while(*s && strchr(delim, *s))
		s++;

	if(!*s) {
		if (!oldword) oldword = kmalloc(300);
		strcpy(oldword,s);
		return 0;
	}

	word = s;
	while(*s && !strchr(delim, *s))
		s++;

	if(*s) {
		*s = 0;
		oldword = s + 1;
	} else {
		oldword = s;
	}

	return word;
}
char *strdup(char *src) {
	int len = strlen(src);
	char *p = kmalloc(len+1);
    memset(p,0,len+1);
	if (!p) return NULL;
	memcpy(p,src,len);
	return p;
}
/*
 * Update 27/9/21
 * Add atoi support
*/
bool isdigit(char c) {
    return c >= '0' && c <= '9';
}
int atoi(char *number) {
    int value = 0;
    while(isdigit(*number)) {
        value *=10;
        value +=(*number)-'0';
        number++;
    }
    return value;
}
extern char* strcat(char* s1, const char* s2)
{
    char* original = s1;

    while (*s1 != '\0')
        s1++;
    while (*s2 != '\0')
        *s1++ = *s2++;
    *s1 = '\0';

    return original;
}

int memcmp(const void * vl, const void * vr, size_t n) {
	const unsigned char *l = vl;
	const unsigned char *r = vr;
	for (; n && *l == *r; n--, l++, r++);
	return n ? *l-*r : 0;
}
bool islower(char chr)
{
    return chr >= 'a' && chr <= 'z';
}

char toupper(char chr)
{
    return islower(chr) ? (chr - 'a' + 'A') : chr;
}
