/* ACPI first non HelinOS old kernel component */
#ifndef ACPI_H
#define ACPI_H
#include <typedefs.h>
#include <multiboot.h>
/* multiboot used on some systems where the ACPI tables doesn't located on the legacy BIOS address space, like on EFI systems */
int initAcpi(multiboot_info_t *forEfi);
void acpiPowerOff();
bool acpiIsOn();
unsigned int *acpiCheckRSDPtr(unsigned int *ptr);
unsigned int *acpiGetRSDPtr(void);
void acpiPostInit();
#endif
