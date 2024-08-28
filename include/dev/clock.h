/*
 * Hardware clock interface.
 * DESCRIPTION:
 * This interface used by TARGET platform to setup it's hardware clock, for example RTC or something.
 * The kernel will use this clock for example for clock_gettime or clock_gettimeofday.
*/
#ifndef CLOCK_H
#define CLOCK_H

// Hardware clock isn't required by the headless code of kernel. Add -DHWCLOCK to enable the hardware clock code.

typedef int time_t;
// Newlib structure :)
struct tm
{
  int	tm_sec;
  int	tm_min;
  int	tm_hour;
  int	tm_mday;
  int	tm_mon;
  int	tm_year;
  int	tm_wday;
  int	tm_yday;
  int	tm_isdst;
#ifdef __TM_GMTOFF
  long	__TM_GMTOFF;
#endif
#ifdef __TM_ZONE
  const char *__TM_ZONE;
#endif
};
void hw_clock_init();
int hw_clock_get(struct tm *time);
int hw_clock_set(struct tm *time);
/*
 * System starts counting time after boot, so we need to separate function
 * to get the system software time, not hardware one.
*/
int clock_get(struct tm *time);
#endif
