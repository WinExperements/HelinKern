// Global implementation
#include <output.h>
#include <stdarg.h>
#include <thread.h>
char *ringBuff;
int ringBuffPtr;
int ringBuffSize;
bool userWrite = false;
bool disableOutput = true; // can be changed from somewhere in kernel
void output_writeInt(int u) {
    	if (u == 0) {
          output_write("0");
          return;
        }
        s32 acc = u;
        char c[32];
        int i = 0;
        while(acc > 0) {
                c[i] = '0' + acc%10;
                acc /= 10;
                i++;
        }
        c[i] = 0;
        char c2[32];
        c2[i--] = 0;
        int j = 0;
        while(i >= 0) {
                c2[i--] = c[j++];
        }
	output_write(c2);
}
void output_printHex(uintptr_t num) {
    	uintptr_t tmp;
	char noZeroes = 1;
	uintptr_t i;
	int howMany = sizeof(num) == 8 ? 56 : 28;
	for (i = howMany; i > 0; i-=4) {
		tmp = (num >> i) & 0xF;
		if (tmp == 0 && noZeroes != 0) continue;
		if (tmp >= 0xA) {
			noZeroes = 0;
			putc(tmp-0xA+'a');
		} else {
			noZeroes = 0;
			putc(tmp+'0');
		}
	}
	tmp = num & 0xF;
	if (tmp >= 0xA) {
		putc(tmp-0xA+'a');
	} else {
		putc(tmp+'0');
	}
}
void kprintf(char *format,...) {
	//if (disableOutput) return;
        output_write("[");
        output_writeInt(clock_getUptimeMsec());
        output_write("] ");
        va_list arg;
	va_start(arg,format);
	while (*format != '\0') {
		if (*format == '%') {
			if (*(format +1) == '%') {
				output_write("%");
			} else if (*(format + 1) == 's') {
				output_write(va_arg(arg,char*));
			} else if (*(format + 1) == 'c') {
				putc(va_arg(arg,int));
			} else if (*(format + 1) == 'd') {
				output_writeInt(va_arg(arg,int));
			} else if (*(format + 1) == 'x') {
				output_printHex(va_arg(arg,int));
			} else if (*(format + 1) == '\0') {
				PANIC("kprintf: next char is null!");
			}
			format++;
		} else {
			putc(*format);
		}
		format++;
	}
	va_end(arg);
}
void putc(char c) {
  if (userWrite) goto put;
  if (CONF_RING_SIZE > 0) {
    if (ringBuffSize == 0) {
      ringBuffSize = (1<<CONF_RING_SIZE);
    }
    if (ringBuff == NULL) goto put;
    ringBuff[ringBuffPtr++] = c;
    if (ringBuffPtr >= ringBuffSize) {
      ringBuffPtr = 0;
    }
    ringBuff[ringBuffPtr] = 0;
  }
put:
  if (disableOutput) return;
  output_putc(c);
}
