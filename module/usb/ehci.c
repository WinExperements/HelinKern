/*
 * USB 2.0 Device driver for HelinKern.
 *
 * This peace of code currently just probes for devices attached to the USB controller itself.
*/
// EHCI types.
#include <typedefs.h>
#include <output.h>
#include <pci/driver.h>
#include <pci/registry.h>
#include <arch/mmu.h>
#include <arch.h>
#include <thread.h>
#include <arch/x86/idt.h>
#include <mm/alloc.h>
#define HCCPARAMS_EECP_MASK (255<<8)
#define HCCPARAMS_EECP_SHIFT 8
#define HCSPARAMS_N_PORTS_MASK          0x0f   // Number of Ports
#define USBLEGSUP                       0x00        // USB Legacy Support Extended Capability
#define UBSLEGCTLSTS                    0x04        // USB Legacy Support Control/Status

// ------------------------------------------------------------------------------------------------
// USB Legacy Support Register

#define USBLEGSUP_HC_OS                 0x01000000  // HC OS Owned Semaphore
#define USBLEGSUP_HC_BIOS               0x00010000  // HC BIOS Owned Semaphore
#define USBLEGSUP_NCP_MASK              0x0000ff00  // Next EHCI Extended Capability Pointer
#define USBLEGSUP_CAPID                 0x000000ff  // Capability ID
char name[] __attribute__((section(".modname"))) = "ehci";
typedef struct usb_cap_reg {
	uint8_t size;
	uint8_t resv;
	uint16_t hciVer;
	uint32_t hcsParams;
	uint32_t hccParams;
	uint64_t hcspPortRout;
} __attribute__((packed)) UsbCapRegister; // USB capability registers structure.
typedef struct usb_op_regs {
	volatile uint32_t usbCmd;	// Comand register.
	volatile uint32_t usbSts;	// Usb Status.
	volatile uint32_t usbInt;	// Usb interrupts.
	volatile uint32_t frameIndex;
	volatile uint32_t ctrlDsSegment;
	volatile uint32_t periodListBase;
	volatile uint32_t asyncListAddr;
	volatile uint32_t resv[9];
	volatile uint32_t config;
	volatile uint32_t ports[];
} UsbOpRegisters;

// Data structures.
// Transfer Descripor.
typedef struct ehci_transfer {
	volatile uint32_t link;
	volatile uint32_t alterLink;
	volatile uint32_t token;
	volatile uint32_t buffer[5];
	volatile uint32_t alterBuffer[5];
	// Non EHCI fields. Used by driver for manipulations.
	bool active;
} EhciTD;
typedef struct ehci_queue_head {
	uint32_t qhLinkPtr;
	uint32_t ch;
	uint32_t caps;
	volatile uint32_t curLink;
	volatile uint32_t nextLink;
	volatile uint32_t alterLink;
	volatile uint32_t token;
	volatile uint32_t buffer[5];
	volatile uint32_t alterBuffer[5];
	bool active;
} EhciQueueHead;

// Defines for driver.
#define PORT_CONNECTION                 (1 << 0)    // Current Connect Status
#define PORT_CONNECTION_CHANGE          (1 << 1)    // Connect Status Change
#define PORT_ENABLE                     (1 << 2)    // Port Enabled
#define PORT_ENABLE_CHANGE              (1 << 3)    // Port Enable Change
#define PORT_OVER_CURRENT               (1 << 4)    // Over-current Active
#define PORT_OVER_CURRENT_CHANGE        (1 << 5)    // Over-current Change
#define PORT_FPR                        (1 << 6)    // Force Port Resume
#define PORT_SUSPEND                    (1 << 7)    // Suspend
#define PORT_RESET                      (1 << 8)    // Port Reset
#define PORT_LS_MASK                    (3 << 10)   // Line Status
#define PORT_LS_SHIFT                   10
#define PORT_POWER                      (1 << 12)   // Port Power
#define PORT_OWNER                      (1 << 13)   // Port Owner
#define PORT_IC_MASK                    (3 << 14)   // Port Indicator Control
#define PORT_IC_SHIFT                   14
#define PORT_TC_MASK                    (15 << 16)  // Port Test Control
#define PORT_TC_SHIFT                   16
#define PORT_WKCNNT_E                   (1 << 20)   // Wake on Connect Enable
#define PORT_WKDSCNNT_E                 (1 << 21)   // Wake on Disconnect Enable
#define PORT_WKOC_E                     (1 << 22)   // Wake on Over-current Enable
#define PORT_RWC                        (PORT_CONNECTION_CHANGE | PORT_ENABLE_CHANGE | PORT_OVER_CURRENT_CHANGE)
// Commands!
#define CMD_RS                          (1 << 0)    // Run/Stop
#define CMD_HCRESET                     (1 << 1)    // Host Controller Reset
#define CMD_FLS_MASK                    (3 << 2)    // Frame List Size
#define CMD_FLS_SHIFT                   2
#define CMD_PSE                         (1 << 4)    // Periodic Schedule Enable
#define CMD_ASE                         (1 << 5)    // Asynchronous Schedule Enable
#define CMD_IOAAD                       (1 << 6)    // Interrupt on Async Advance Doorbell
#define CMD_LHCR                        (1 << 7)    // Light Host Controller Reset
#define CMD_ASPMC_MASK                  (3 << 8)    // Asynchronous Schedule Park Mode Count
#define CMD_ASPMC_SHIFT                 8
#define CMD_ASPME                       (1 << 11)   // Asynchronous Schedule Park Mode Enable
#define CMD_ITC_MASK                    (255 << 16) // Interrupt Threshold Control
#define CMD_ITC_SHIFT                   16

// Statuses.
#define STS_USBINT                      (1 << 0)    // USB Interrupt
#define STS_ERROR                       (1 << 1)    // USB Error Interrupt
#define STS_PCD                         (1 << 2)    // Port Change Detect
#define STS_FLR                         (1 << 3)    // Frame List Rollover
#define STS_HSE                         (1 << 4)    // Host System Error
#define STS_IOAA                        (1 << 5)    // Interrupt on Async Advance
#define STS_HCHALTED                    (1 << 12)   // Host Controller Halted
#define STS_RECLAMATION                 (1 << 13)   // Reclamation
#define STS_PSS                         (1 << 14)   // Periodic Schedule Status
#define STS_ASS                         (1 << 15)   // Asynchronous Schedule Statu

#define PTR_TERMINATE                   (1 << 0)

#define PTR_TYPE_MASK                   (3 << 1)
#define PTR_ITD                         (0 << 1)
#define PTR_QH                          (1 << 1)
#define PTR_SITD                        (2 << 1)
#define PTR_FSTN                        (3 << 1)

// TD Token
#define TD_TOK_PING                     (1 << 0)    // Ping State
#define TD_TOK_STS                      (1 << 1)    // Split Transaction State
#define TD_TOK_MMF                      (1 << 2)    // Missed Micro-Frame
#define TD_TOK_XACT                     (1 << 3)    // Transaction Error
#define TD_TOK_BABBLE                   (1 << 4)    // Babble Detected
#define TD_TOK_DATABUFFER               (1 << 5)    // Data Buffer Error
#define TD_TOK_HALTED                   (1 << 6)    // Halted
#define TD_TOK_ACTIVE                   (1 << 7)    // Active
#define TD_TOK_PID_MASK                 (3 << 8)    // PID Code
#define TD_TOK_PID_SHIFT                8
#define TD_TOK_CERR_MASK                (3 << 10)   // Error Counter
#define TD_TOK_CERR_SHIFT               10
#define TD_TOK_C_PAGE_MASK              (7 << 12)   // Current Page
#define TD_TOK_C_PAGE_SHIFT             12
#define TD_TOK_IOC                      (1 << 15)   // Interrupt on Complete
#define TD_TOK_LEN_MASK                 0x7fff0000  // Total Bytes to Transfer
#define TD_TOK_LEN_SHIFT                16
#define TD_TOK_D                        (1 << 31)   // Data Toggle
#define TD_TOK_D_SHIFT                  31

#define USB_PACKET_OUT                  0           // token 0xe1
#define USB_PACKET_IN                   1           // token 0x69
#define USB_PACKET_SETUP                2           // token 0x2d

// Extended.
#define QH_CH_DEVADDR_MASK              0x0000007f  // Device Address
#define QH_CH_INACTIVE                  0x00000080  // Inactive on Next Transaction
#define QH_CH_ENDP_MASK                 0x00000f00  // Endpoint Number
#define QH_CH_ENDP_SHIFT                8
#define QH_CH_EPS_MASK                  0x00003000  // Endpoint Speed
#define QH_CH_EPS_SHIFT                 12
#define QH_CH_DTC                       0x00004000  // Data Toggle Control
#define QH_CH_H                         0x00008000  // Head of Reclamation List Flag
#define QH_CH_MPL_MASK                  0x07ff0000  // Maximum Packet Length
#define QH_CH_MPL_SHIFT                 16
#define QH_CH_CONTROL                   0x08000000  // Control Endpoint Flag
#define QH_CH_NAK_RL_MASK               0xf0000000  // Nak Count Reload
#define QH_CH_NAK_RL_SHIFT              28

// Endpoint Capabilities
#define QH_CAP_INT_SCHED_SHIFT          0
#define QH_CAP_INT_SCHED_MASK           0x000000ff  // Interrupt Schedule Mask
#define QH_CAP_SPLIT_C_SHIFT            8
#define QH_CAP_SPLIT_C_MASK             0x0000ff00  // Split Completion Mask
#define QH_CAP_HUB_ADDR_SHIFT           16
#define QH_CAP_HUB_ADDR_MASK            0x007f0000  // Hub Address
#define QH_CAP_PORT_MASK                0x3f800000  // Port Number
#define QH_CAP_PORT_SHIFT               23
#define QH_CAP_MULT_MASK                0xc0000000  // High-Bandwidth Pipe Multiplier
#define QH_CAP_MULT_SHIFT               30

typedef struct _ctrl {
	UsbCapRegister *caps;
	UsbOpRegisters *op;
	EhciTD *td;
	EhciQueueHead *qh;
} EhciController;
// Transfer queues.
static EhciQueueHead *EhciAllocQH(EhciController *ctrl) {
	EhciQueueHead *end = ctrl->qh + 8;
	for (EhciQueueHead *qh = ctrl->qh; qh != end; ++qh) {
		if (!qh->active) {
			qh->active = true;
			return qh;
		}
	}
	return NULL;
}

// Controller side.
static void EhciPortWrite(volatile uint32_t *reg,uint32_t data) {
	uint32_t status = *reg; // read.
	status |= data;
	status &= ~(PORT_RWC);
	*reg = status; // write back.
}
static void EhciPortClear(volatile uint32_t *reg,uint32_t data) {
	uint32_t status = *reg;
	status &= ~PORT_RWC;
	status &= ~data;
	status |= PORT_RWC & data;
	*reg = status;
}
// Port manipulation.
static uint32_t EhciResetPort(UsbOpRegisters *op,uint32_t port) {
	volatile uint32_t *reg = &op->ports[port];
	arch_sti();
	// Enable power BEFORE the reset of port.
	*reg |= PORT_POWER;
	kwait(50);
	EhciPortWrite(reg,PORT_RESET);
	kwait(50);
	EhciPortClear(reg,PORT_RESET);
	kwait(20);
	*reg |= PORT_POWER;
	//kprintf("Old(readed): 0x%x",*reg);
//	kprintf(" new(readed): 0x%x\n",*reg);
	uint32_t status = 0;
	for (int i = 0; i < 50; i++) {
		/*status = *reg;
		status |= PORT_POWER;
		*reg = status;*/
		kwait(10);
		status = *reg;
		//kprintf("Status: 0x%x\n",status);
		if (~status & PORT_CONNECTION) {
			//kprintf("EHCI: Port #%d doesn't connected\n",port);
			break;
		}
		if (status & (PORT_ENABLE_CHANGE | PORT_CONNECTION_CHANGE)) {
			//kprintf("Clearing PORT_ENABLE_CHANGE\n");
			EhciPortClear(reg, PORT_ENABLE_CHANGE | PORT_CONNECTION_CHANGE);
			continue;
		}
		if (status & PORT_ENABLE) {
			break;
		}
	}
	EhciPortWrite(reg,PORT_POWER);
	return status;
}
static void SetupController(void *address,unsigned int devID) {
	UsbCapRegister *caps = (UsbCapRegister *)(uintptr_t)address;
	arch_mmu_mapPage(NULL,(vaddr_t)caps,(paddr_t)caps,3);
	UsbOpRegisters *op = (UsbOpRegisters *)(uintptr_t)(address + caps->size);
	arch_mmu_mapPage(NULL,(vaddr_t)op,(paddr_t)op,3);
	int portsN = caps->hcsParams & HCSPARAMS_N_PORTS_MASK;
	op->usbInt = 0;
	unsigned int eecp = (caps->hccParams & HCCPARAMS_EECP_MASK) >> HCCPARAMS_EECP_SHIFT;
	if (eecp >= 0x40) {
		unsigned int legsup = PciRead32(devID,eecp + USBLEGSUP);
		if (legsup & USBLEGSUP_HC_BIOS) {
			kprintf("taking controll over the USB controller\n");
			PciWrite32(devID,eecp+USBLEGSUP,legsup | USBLEGSUP_HC_OS);
			// Wait for the BIOS semaphrone to be unlocked.
			int clocks = 0;
			for (;;) {
				legsup = PciRead32(devID,eecp + USBLEGSUP);
				if (~legsup & USBLEGSUP_HC_BIOS && legsup & USBLEGSUP_HC_OS) {
					kprintf("Ownership transmited\n");
					break;
				}
				clocks++;
				if (clocks >= 15000) {
					kprintf("Ownership taking error. Timeout\n");
					return;
				}
			}
		}
	}
	// Disable interrupts on this controller.
	op->usbInt = 0;
	// Prepare the queue heads
	EhciController *ctrl = kmalloc(sizeof(EhciController));
	memset(ctrl,0,sizeof(EhciController));
	ctrl->qh = kmalloc(sizeof(EhciQueueHead)*8);
	EhciQueueHead *qh = EhciAllocQH(ctrl);
	qh->qhLinkPtr = (uint32_t)(uintptr_t)arch_mmu_getPhysical(qh) | PTR_QH;
	qh->ch = QH_CH_H;
	qh->caps = 0;
	qh->curLink = 0;
	qh->nextLink = PTR_TERMINATE;
	qh->alterLink = 0;
	qh->token = 0;
	for (int i = 0; i < 5; ++i) {
		qh->buffer[i] = 0;
		qh->alterBuffer[i] = 0;
	}
	// Allocate ASYNC Queue Head.
	EhciQueueHead *asyncQh = EhciAllocQH(ctrl);
	asyncQh->qhLinkPtr = PTR_TERMINATE;
	asyncQh->ch = 0;
	asyncQh->caps = 0;
	asyncQh->curLink = 0;
	asyncQh->nextLink = PTR_TERMINATE;
	asyncQh->alterLink = 0;
	asyncQh->token = 0;
	for (int i = 0; i < 5; ++i) {
		asyncQh->buffer[i] = 0;
		asyncQh->alterBuffer[i] = 0;
	}
	// Set the EHCI controller registers to point to the queue heads.
	uint32_t *frameList = kmalloc(1024 * sizeof(uint32_t));
	paddr_t asyncQhPhys = (paddr_t)arch_mmu_getPhysical(asyncQh);
	op->frameIndex = 0;
	for (int i = 0; i < 1024; ++i) {
		frameList[i] = PTR_QH | (uint32_t)(uintptr_t)asyncQhPhys;
	}
	op->periodListBase = (uint32_t)(uintptr_t)arch_mmu_getPhysical(frameList);
	op->asyncListAddr = (uint32_t)(uintptr_t)arch_mmu_getPhysical(qh);
	op->ctrlDsSegment = 0;
	// Scan bus.
	op->usbSts = 0x3f;
	kprintf("Enabling USB controller\n");
	op->usbCmd = (8 << CMD_ITC_SHIFT) | CMD_PSE | CMD_ASE | CMD_RS;
	// Wait for the port respons?
	while(op->usbSts & STS_HCHALTED);
	kprintf("got response from usb!\n");
	op->config = 1;
	portsN = caps->hcsParams & HCSPARAMS_N_PORTS_MASK;
	if (portsN == 0) {
		if (op->ports[0] != NULL) {
			kprintf("EHCI: Controller reported 0 ports, but the port status register is available. Assuming that we have 1 port available\n");
			portsN = 1;
		}
	}
	kprintf("%s: EHCI Controller Reset Successfull. PortN: %d\n",__func__,portsN);
	for (int i = 0; i < portsN; i++) {
		uint32_t status = EhciResetPort(op,i);
		if (status & PORT_ENABLE) {
			kprintf("USB Port #%d enabled\n",i);
		}
	}
	uint16_t irq = PciRead16(devID,PCI_CONFIG_INTERRUPT_PIN);
	kprintf("This controller IRQ: 0x%x\n",IRQ0 + irq);
	// Allow interrupts!
//	op->usbInt = (1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<4) | (1<<5);
}

// Searching.
static int controllerID;
static bool PciVisit(PciDeviceInfo *info) {
    if (((info->classCode << 8) | info->subclass) == PCI_SERIAL_USB &&
		    info->progIntf == PCI_SERIAL_USB_EHCI) {
	    // Okay. Found.
	    PciBar bar;
	    PciGetBar(&bar,info->baseID,0);
	    if (bar.flags & PCI_BAR_IO) {
		    kprintf("Controller #%d had invalid I/O config.\n",controllerID);
		    return false;
		}
	    kprintf("Initializing USB EHCI controller #%d\n",controllerID);
	    SetupController(bar.u.address,info->baseID);
	    controllerID++;
    }
    return false;
}

static int ehciID = 0;

void usb_ehci_init() {
	//clock_setShedulerEnabled(false);
	int controllerID = 0;
	PciForeach(PciVisit);
}
static void module_main() {
        kprintf("Super puper ehci mode is powering on. Maybe\n");
        usb_ehci_init();
}
