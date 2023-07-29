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
		if (oldword == NULL) {
            oldword = strdup(s);
        } else {
            kfree(oldword);
            oldword = strdup(s);
        }
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

// New API
char* strinv(char* str)
{
    int i;
    uint32_t n = strlen(str);
    char buf[n + 2];
    char* cur = buf;

    for (i = n - 1; i >= 0; --i) {
        *cur++ = str[i];
    }
    *cur++ = '\0';

    strcpy(str, buf);

    return str;
}
char* itoa(uint32_t value, char* str, uint32_t base)
{
    char* original = str;
    char digit;

    do {
        digit = value % base;
        value = value / base;
        if (digit < 10) {
            *str++ = digit | 0x30; /* number */
        } else if (digit < 16) {
            *str++ = ((digit - 10) | 0x40) + 1; /* alpha */
        } else {
            *str++ = '?';
        }
    } while (value > 0);

    if (base == 16) {
        /* hexedecimal integer */
        *str++ = 'x';
        *str++ = '0';
    } else if (base == 8) {
        /* octal integer */
        *str++ = 'o';
        *str++ = '0';
    } else if (base == 2) {
        /* binary integer */
        *str++ = 'b';
        *str++ = '0';
    }
    *str++ = '\0';

    strinv(original);

    return str;
}

unsigned int sprintf(char* s1, const char* s2, ...)
{
    va_list list;
    va_start(list, s2);

    return vsprintf(s1, s2, list);
}
unsigned int snprintf(char* s1, unsigned int n, const char* s2, ...)
{
    va_list list;
    va_start(list, s2);

    return vsnprintf(s1, n, s2, list);
}
unsigned int vsprintf(char* s1, const char* s2, va_list list)
{
    return vsnprintf(s1, 4 * 1024, s2, list);
}

unsigned int vsnprintf(char* s1, unsigned int n, const char* s2, va_list list)
{
    uint32_t j = 0;
    size_t count = 0;
    char number[32];
    char* cur = s1;
    char* str;

    while (s2[j] != '\0' && j < n) {
        if (s2[j] != '%') {
            /* text */
            *cur++ = s2[j++];
        } else if (s2[j] == '%') {
            /* control character */
            switch (s2[++j]) {
            case 'c':
                /* character */
                *cur++ = va_arg(list, int);
                break;
            case 'd':
                /* unsigned decimal */
                itoa(va_arg(list, uint32_t), number, 10);
                strcpy(cur, number);
                cur += strlen(number);
                break;
            case 'x':
                /* unsigned hexedecimal */
                itoa(va_arg(list, uint32_t), number, 16);
                strcpy(cur, number);
                cur += strlen(number);
                break;
            case 's':
                /* string */
                str = va_arg(list, char*);
                strcpy(cur, str);
                cur += strlen(str);
                break;
            }
            j += 1;
        }
    }

    count = ((size_t)cur - (size_t)s1);
    *cur++ = '\0';

    va_end(list);

    return count;
}
