#ifndef SMP_H
#define SMP_H

#include <typedefs.h>
/* SMP support, aka multiprocessor support */
// This component is very required to boost perfomence on multicore devices
// On such platforms as X86 and AMD64 we gona use APIC

#define MPF_SIGNATURE ('_'<<(3 * 8) | 'P'<<(2 * 8) | 'M'<<(1 * 8) | '_')

struct mpf_struct {
	uint32_t signature;		/* "_MP_" */
	uint32_t conf_physaddr;		/* MP configuration table pointer */
	uint8_t length;			/* Structure length in 16-byte units */
	uint8_t version;		/* version1.1=0x01, version1.4=0x04 */
	uint8_t checksum;		/* Sum of structure's bytes */
	uint8_t feature1;		/* Default configuration used? */
	uint8_t feature2_reserved:7,	/* Reserved */
		imcr:1;			/* Obsolete PIC mode implemented? */
	uint8_t feature3;		/* Reserved */
	uint8_t feature4;		/* Reserved */
	uint8_t feature5;		/* Reserved */
} __attribute__((packed));

/*
 * MP Configuration table header
 */

#define MPC_SIGNATURE ('P'<<(3 * 8) | 'M'<<(2 * 8) | 'C'<<(1 * 8) | 'P')

struct mpc_table {
	uint32_t signature;		/* "PCMP" */
	uint16_t length;		/* Base Table length + header */
	uint8_t version;		/* version1.1=0x01, version1.4=0x04 */
	uint8_t checksum;		/* mpf checksum */
	char oem[8];			/* OEM ID */
	char product[12];		/* Product ID/family */
	uint32_t oem_physaddr;		/* OEM table physical pointer, or 0 */
	uint16_t oem_size;		/* OEM table size in bytes, or 0 */
	uint16_t entries;		/* Num of table's entries */
	uint32_t lapic_base;		/* Obsolete LAPIC base - use MSRs */
	uint16_t ext_length;		/* Extended entries length, or 0 */
	uint8_t ext_checksum;		/* Extended entries checksum, or 0 */
	uint8_t reserved;
} __attribute__((packed));

/*
 * MP Configuration table entries
 */

struct mpc_cpu {
	uint8_t entry;			/* Entry type (processor) */
	uint8_t lapic_id;		/* This processor's lapic ID */
	uint8_t lapic_ver;		/* This proecessor's lapic version */
	uint8_t enabled:1,		/* Set if this processor is usable */
		bsc:1,			/* Set for the bootstrap processor */
		flags_reserved:6;	/* Reserved */
	uint32_t signature;		/* Signature (stepping, model, family */
	uint32_t flags;			/* Flags as returned by CPUID */
	uint64_t reserved;		/* Reserved */
} __attribute__((packed));

struct mpc_bus {
	uint8_t entry;			/* Entry type (bus) */
	uint8_t id;			/* Bus ID */
	char type[6];			/* Bus Type string */
} __attribute__((packed));

struct mpc_ioapic {
	uint8_t entry;			/* Entry type (ioapic) */
	uint8_t id;			/* The ID of this I/O APIC */
	uint8_t version;		/* I/O APIC's version register */
	uint8_t enabled:1,		/* If zero, this I/O APIC is unusable */
		flags_reserved:7;	/* Reserved */
	uint32_t base;			/* This I/O APIC base address */
} __attribute__((packed));

struct mpc_irq {
	uint8_t entry;			/* Entry type (I/O interrupt entry) */
	uint8_t type;			/* Interrupt type */
	uint16_t polarity:2,		/* Polarity of APIC I/O input signal */
		trigger:2,		/* Trigger mode */
		reserved:12;		/* Reserved */
	uint8_t src_busid;		/* Interrupt source bus */
	uint8_t src_busirq;		/* Source bus irq */
	uint8_t dst_ioapicid;		/* Destination I/O APIC ID */
	uint8_t dst_ioapicpin;		/* Destination I/O APIC INTINn pin */
} __attribute__((packed));

struct mpc_linterrupt {
	uint8_t entry;			/* Entry type (local interrupt entry) */
	uint8_t type;			/* Interrupt type */
	uint16_t polarity:2,		/* Polarity of APIC I/O input signal */
		trigger:2,		/* Trigger mode */
		reserved:12;		/* Reserved */
	uint8_t src_busid;		/* Interrupt source bus */
	uint8_t src_busirq;		/* Source bus irq */
	uint8_t dst_lapic;		/* Destination local APIC ID */
	uint8_t dst_lapicpin;		/* Destination local APIC LINTINn pin */
} __attribute__((packed));

enum mp_entry {
	MP_PROCESSOR = 0,
	MP_BUS,
	MP_IOAPIC,
	MP_IOINTERRUPT,
	MP_LINTERRUPT,
};

enum mp_irqtype {
	MP_INT = 0,			/* IOAPIC provided vector */
	MP_NMI,
	MP_SMI,
	MP_ExtINT,			/* 8259A provided vector */
};

extern int mp_isa_busid;

extern int nr_mpcirqs;

void smp_init();
void apic_init();
void smp_post_init();
int smp_getCPUCount();
#endif
