#pragma once
#include <typedefs.h>
/* Coreboot support for headless version of kernel.
 * defines only coreboot structures and methods to parse it.
*/
#include <dev/fb.h>
void coreboot_init(void *addr);
int coreboot_findLargestRegion(int type);
bool coreboot_initFB(fbinfo_t *info);
char *coreboot_findMotherboardVendor();
