#ifndef REBOOT_H
#define REBOOT_H

#define RB_DISABLE_CAD 0
#define RB_ENABLE_CAD 0x89abcdef
#define RB_HALT_SYSTEM 0xcdef0123
#define RB_POWER_OFF 0x4321fedc
#define RB_AUTOBOOT 0x1234567

int reboot(int reason);

#endif
