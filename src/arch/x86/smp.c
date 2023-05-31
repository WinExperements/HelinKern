#include <typedefs.h>
#include <arch.h>
#include <output.h>
#include <debug.h>
#include <arch/x86/smp.h>
#include <arch/x86/cpuid.h>
#include <arch/x86/io.h>
#include <lib/string.h>
#include <mm/alloc.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define CPUS_MAX 64
#define IOAPICS_MAX 8

static int nr_ioapics;

static int nr_cpus = 1;

int mp_isa_busid = -1;

int nr_mpcirqs;
#define MAX_IRQS	(0xff - 0x1f)

static bool check_apic() {
	uint32_t eax,ebx,ecx,edx;
	cpuid(1,&eax,&ebx,&ecx,&edx);
	return edx & EDX_APIC;
}

static bool checkMsr() {
	uint32_t eax,ebx,ecx,edx;
	cpuid(1,&eax,&ebx,&ecx,&edx);
	return edx & EDX_MSR;
}

static void getMsr(uint32_t msr,uint32_t *lo,uint32_t *hi) {
    asm volatile("rdmsr" : "=a" (*lo), "=d"(*hi) : "c"(msr));
}

static void setMsr(uint32_t msr,uint32_t lo,uint32_t hi) {
    asm volatile("wrmsr" : : "a" (lo), "d"(hi), "c"(msr));
}

static void cpu_set_apic_base(uintptr_t apic) {
   uint32_t edx = 0;
   uint32_t eax = (apic & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   edx = (apic >> 32) & 0x0f;
#endif 
   setMsr(IA32_APIC_BASE_MSR, eax, edx);
}

static uintptr_t cpu_get_apic_base() {
   uint32_t eax, edx;
   getMsr(IA32_APIC_BASE_MSR, &eax, &edx);
#ifdef __PHYSICAL_MEMORY_EXTENSION__
   return (eax & 0xfffff000) | ((edx & 0x0f) << 32);
#else
   return (eax & 0xfffff000);
#endif
}

static void WriteReg(void *p, uint32_t data)
{
    *(volatile uint32_t *)(p) = data;
}

static uint32_t ReadRegister(void *p)
{
    return *(volatile uint32_t *)(p);
}

static uint8_t mpf_checksum(void *mp, uint32_t len)
{
	uint8_t sum = 0;
	uint8_t *buf = mp;

	while (len--)
		sum += *buf++;

	return sum;
}

static struct mpf_struct *search_for_mpf(void *base, uint32_t len)
{
	struct mpf_struct *mpf = base;
	uint8_t checksum;

	for (; len > 0; mpf += 1, len -= sizeof(*mpf)) {
		if (len < sizeof(*mpf))
			continue;
		if (mpf->signature != MPF_SIGNATURE)
			continue;
		if (mpf->length != 0x01)
			continue;
		if (mpf->version != 0x01 && mpf->version != 0x04)
			continue;
		checksum = mpf_checksum(mpf, sizeof(*mpf));
		if (checksum != 0) {
			kprintf("MP: buggy MP floating pointer struct at 0x%x "
			       "with checksum = %d\n", mpf, checksum);
			continue;
		}

		DEBUG("MP: Found an MP pointer at 0x%x\r\n", mpf);
		return mpf;
	}

	return NULL;
}

static struct mpf_struct *get_mpf(void)
{
	uintptr_t ebda;
	struct mpf_struct *mpf;

	ebda = (*(uint16_t *)((void *)(0x40e)) << 4);
	mpf = search_for_mpf((void *)ebda, 0x400);
	if (mpf != NULL)
		return mpf;

	mpf = search_for_mpf((void *)(639 * 0x400), 0x400);
	if (mpf != NULL)
		return mpf;

	mpf = search_for_mpf((void *)0xF0000, 0x10000);
	if (mpf != NULL)
		return mpf;

	return NULL;
}

static int mpc_check(struct mpc_table *mpc)
{
	uint8_t checksum;

	if (mpc->signature != MPC_SIGNATURE) {
		kprintf("MP: Wrong configuration table signature = 0x%x\n",
		       mpc->signature);
		return 0;
	}
	if (mpc->version != 0x01 && mpc->version != 0x04) {
		kprintf("MP: Wrong configuration table version = 0x%x\n",
		       mpc->version);
		return 0;
	}
	checksum = mpf_checksum(mpc, mpc->length);
	if (checksum != 0) {
		kprintf("MP: buggy configuration table checksum = 0x%x\n",
		       checksum);
		return 0;
	}

	/* Ignore checking the LAPIC base address. Reading the APIC base
	 * from the MP table is 12-years obsolete: it was done before
	 * Intel's creating of the APIC Base Address MSR in 686+ models */

	return 1;
}

static void parse_cpu(void *addr)
{
	struct mpc_cpu *cpu = addr;
	static bool bsc_entry_filled;

	if (!cpu->enabled) {
        DEBUG_N("CPU disabled by firmware, adding as disabled count to be enabled\r\n");
        ++nr_cpus;
        return;
    }

	if (cpu->bsc) {
		if (bsc_entry_filled)  {
			kprintf("Two `bootstrap' cores in the MP tables! "
			      "Either the BIOS or our parser is buggy.");
            		PANIC("");
		}

		bsc_entry_filled = 1;
		return;
	}

	if (nr_cpus >= CPUS_MAX) {
		kprintf("Only %d logical CPU cores supported\n", nr_cpus);
        	PANIC("");
	}


	++nr_cpus;
}

static void parse_ioapic(void *addr)
{
	struct mpc_ioapic *ioapic = addr;

	if (!ioapic->enabled)
		return;

	if (nr_ioapics >= IOAPICS_MAX) {
		kprintf("Only %d IO APICs supported", IOAPICS_MAX);
        	PANIC("");
	}

	/* We read the version from the chip itself instead
	 * of reading it now from the mptable entries */

	++nr_ioapics;
}

static void parse_irq(void *addr)
{
	struct mpc_irq *irq = addr;

	if (nr_mpcirqs >= MAX_IRQS) {
		kprintf("Only %d IRQ sources supported", MAX_IRQS);
        	PANIC("AA");
	}

	++nr_mpcirqs;
}

static void parse_bus(void *addr)
{
	struct mpc_bus *bus = addr;

	/* Only the ISA bus is needed for now */
	if (memcmp("ISA", bus->type, sizeof("ISA") - 1) == 0)
		mp_isa_busid = bus->id;

	return;
}

/*
 * Parse the MP configuration table, copying needed data
 * to our own system-wide mp_*[] tables.
 */
static int parse_mpc(struct mpc_table *mpc)
{
	uint8_t *entry;

	entry = (uint8_t *)(mpc + 1);	/* WARN: possibly un-mapped! */
    DEBUG("Parsing %d entries\r\n",mpc->entries);

	for (int i = 0; i < mpc->entries; i++) {
		switch (*entry) {
		case MP_PROCESSOR:
			parse_cpu(entry);
			entry += sizeof(struct mpc_cpu);
			break;
		case MP_BUS:
			parse_bus(entry);
			entry += sizeof(struct mpc_bus);
			break;
		case MP_IOAPIC:
			parse_ioapic(entry);
			entry += sizeof(struct mpc_ioapic);
			break;
		case MP_IOINTERRUPT:
			parse_irq(entry);
			entry += sizeof(struct mpc_irq);
			break;
		case MP_LINTERRUPT:
			entry += sizeof(struct mpc_linterrupt);
			break;
		default:
			kprintf("MP: Unknown conf table entry type = %d\n",
			      *entry);
			return 0;
		}
	}

	return 1;
}

void apic_init() {
	DEBUG_N("Initializing APIC\r\n");
	if (!checkMsr()) {
		DEBUG_N("No MSR support detected via CPUID. Cannot process\r\n");
		return;
	}
	if (!check_apic()) {
		DEBUG_N("No APIC support detected\r\n");
		return;
	}
    DEBUG_N("Disabling PIC\r\n");
    outb(0x21, 0xff);            // Disable all IRQs
   	outb(0xa1, 0xff);             // Disable all IRQs
   	outb(0x20, 0x11);
    outb(0xa0, 0x11);
    outb(0x21, 0x20);
    outb(0xa1, 0x28);
    outb(0x21, 0x04);
    outb(0xa1, 0x02);
    outb(0x21, 0x01);
   	outb(0xa1, 0x01);
    DEBUG_N("Enabling APIC\r\n");
    cpu_set_apic_base(cpu_get_apic_base());
    WriteReg((void *)0xF0, ReadRegister((void *)0xF0) | 0x100);
}
void smp_init() {
    DEBUG_N("Other side of code, must be called in arch_post_init\r\n");
}
void smp_post_init() {
    DEBUG_N("SMP initializing\r\n");
    struct mpf_struct *mpf;
	struct mpc_table *mpc;

	mpf = get_mpf();
    if (!mpf) {
        DEBUG_N("No MPF pointer found\r\n");
        return;
    }
    if (mpf->feature1) {
		DEBUG_N("MP: Spec `default configuration' is not supported\r\n");
        return;
    }

	if (mpf->conf_physaddr == 0) {
		DEBUG_N("MP: Spec configuration table does not exist\r\n");
        return;
    }
    mpc = (struct mpc_table *)mpf->conf_physaddr;
    if (!mpc_check(mpc)) {
		PANIC("Buggy MP conf table header");
	}

	if (!parse_mpc(mpc)) {
		PANIC("Can not parse MP conf table");
	}
    DEBUG("Total count of usable CPU(s): %d\r\n",nr_cpus);
}
