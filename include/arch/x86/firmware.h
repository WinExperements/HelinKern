#pragma once
// Various firmware detection and information mechanisms.
// Like SMBIOS
void x86_smbios_init(); // find smbios.
void x86_smbios_printInfo(int type);
