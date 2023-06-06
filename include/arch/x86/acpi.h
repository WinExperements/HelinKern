/* ACPI first non HelinOS old kernel component */
#ifndef ACPI_H
#define ACPI_H
#include <typedefs.h>
int initAcpi();
void acpiPowerOff();
bool acpiIsOn();
#endif
