// RTC hardware clock for the x86.
#include <typedefs.h>
#include <arch/x86/io.h>
#include <dev/clock.h>
#include <output.h>
// RTC defines.
#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

int rtc_busy() {
	outb(CMOS_ADDR,0x0A);
	uint32_t status = inb(CMOS_DATA);
	return (status & 0x80);
}

void hw_clock_init() {
	// The X86 scheduler clock and boot time clock calculation code doesn't use RTC, so nothing to init.
}

uint8_t rtc_read(int reg) {
	outb(CMOS_ADDR,reg);
	return inb(CMOS_DATA);
}

// Convertion for the struct tm
int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int hw_clock_get(struct tm *to) {
	// Access checks.
	if (to == NULL) return false;
	while (rtc_busy()) {
		kprintf("waiting for rtc to become non busy\r\n");
	}
	to->tm_sec = rtc_read(0x00);
	to->tm_min = rtc_read(0x02);
	to->tm_hour = rtc_read(0x04);
	to->tm_mday = rtc_read(0x07);
	to->tm_mon = rtc_read(0x08);
	to->tm_year = rtc_read(0x09);
	uint8_t regB = rtc_read(0x0B);
	if (!(regB & 0x04)) {
		to->tm_sec = (to->tm_sec & 0x0F) + ((to->tm_sec / 16) * 10);
		to->tm_min = (to->tm_min & 0x0F) + ((to->tm_min / 16) * 10);
		to->tm_hour = (to->tm_hour & 0x0F) + ((to->tm_hour / 16) * 10);
		to->tm_mday = (to->tm_mday & 0x0F) + ((to->tm_mday / 16) * 10);
		to->tm_mon = (to->tm_mon & 0x0F) + ((to->tm_mon / 16) * 10);
		to->tm_year = (to->tm_year & 0x0F) + ((to->tm_year / 16) * 10);
	}
	to->tm_year = to->tm_year + 2000;
	to->tm_year = to->tm_year - 1900;
	int days_in_month[] = {0, 31, 28 + is_leap_year(to->tm_year + 1900), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    	to->tm_yday = 0;
    	for (int i = 0; i < to->tm_mon; ++i) {
        	to->tm_yday += days_in_month[i];
    	}
    	to->tm_yday += to->tm_mday - 1; // Adjust for zero-based indexing

    	// Calculate tm_wday using Zeller's congruence
    	int k = to->tm_year % 100;
    	int j = to->tm_year / 100;
    	int m = to->tm_mon + 1;
    	int q = to->tm_mday;
    	int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    	to->tm_wday = (h + 5) % 7; // Convert to Sunday = 0, Monday = 1, ..., Saturday = 6
	return true;
}
int hw_clock_set(struct tm *to) {
	return 1;
}
