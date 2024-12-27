#include <output.h>
#include <mm/alloc.h>
#include <pci/pci.h>
#include <pci/driver.h>
#include <audio/ac97/ac97.h>
#include <arch.h>
#include <arch/x86/io.h>
#include <dev.h>
#include <arch/mmu.h>
#include <kernel.h>
#include <lib/fifosize.h>
#include <dev/audio.h>
static uint32_t baseID;
static uint16_t bar0;
static uint16_t bar1;
static Ac97_BufferEntry buffers[32];
static uintptr_t buffer_phys;
static uint8_t *buffersPtr[33]; // audio driver physical data buffers?
static uintptr_t buffersPtrPhys[33];
static uint16_t controller_caps = 0;
static uintptr_t ac97_irq_handler(uintptr_t regs);
static AudioDevice *ac97_dev;
#define AC97_BUFF_SET_SIZE(cl,v) ((cl) = (v) & 0xFFFF)
static bool ac97_scan_handler(PciDeviceInfo *dev) {
  if (dev->classCode == 0x04 && dev->subclass == 0x01) {
    baseID = dev->baseID;
    return true;
  }
  return false;
}

static void ac97_set_output_volume(uint8_t offset,uint8_t volume) {
  if (volume == 0) {
    // Mute.
    outw(bar0 + offset,0x8000);
  } else {
    volume = ((100-volume)*31/100);
    outw(bar0+offset,((volume) | (volume<<8)));
  }
}

static void ac97_playBuffers() {
  // this function will be called within interrupts enabled.
  kprintf("ac97: playing buffers\n");
  //outb(bar1+0x1B,0x0);
  //outb(bar1+0x1B,0x2);
  for (int i = 0; i < 32; i++) {
    buffers[i].address = buffersPtrPhys[i];
    buffers[i].size = 0x1000;
    buffers[i].interrupt = 1;
  }
  if ((controller_caps & 0x1) == 0x1) {
    outw(bar0+0x2C,44100);
    outw(bar0+0x2E,44100);
    outw(bar0+0x30,44100);
    outw(bar0+0x32,44100);
  }
//  buffers[31].last = 1;
  outb(bar1+0x1B,inb(bar1+0x1B) | (1<<1));
  int ticks = 0;
  while((inb(bar1+0x1B) & 0x2) == 0x2) {
    asm volatile("nop");
    if (ticks > 50) {
      kprintf("ac97: general device failure.\n");
      return;
    }
    kwait(20);
    ticks++;
  }
  outb(bar1+0x1B,inb(bar1+0x1B) | (1<<3) | (1<<4));
  outl(bar1+0x10,(uint32_t)buffer_phys);
  outb(bar1+0x15,31);
  outw(bar1+0x16,0x1C);
  kprintf("ac97: start playing.\n");
  // Start playing!
  outb(bar1+0x1B,inb(bar1+0x1B) | (1<<0));
}


static bool ac97_driver_stop() {
  // Stop playback.
  outb(bar1+0x1B,0);
  return true;
}
static bool ac97_driver_play(AudioDevice *dev) {
	ac97_playBuffers();
	return true;
}
static int ac97_driver_setVolume(int dev,int vol) {
	switch(dev) {
		case SND_VOL_OUTPUT_SPEAKER:
			ac97_set_output_volume(0x02,vol);
			return 0;
		
	}
	return -1;
}
void ac97_init() {
  // Scan the PCI bus.
  PciForeach(ac97_scan_handler);
  if (baseID != 0) {
    kprintf("AC97 found!\n");
    bar0 = PciRead16(baseID,PCI_CONFIG_BAR0);
    bar0 -= 1;
    bar1 = PciRead16(baseID,PCI_CONFIG_BAR1);
    bar1 -= 1;
    uint8_t irq = PciRead8(baseID,PCI_CONFIG_INTERRUPT_LINE);
    PciBar bar1a;
    PciGetBar(&bar1a,baseID,1);
    // Enable bus mastering and i/o.
    uint16_t Control = PciRead16(baseID,PCI_CONFIG_COMMAND);
    Control |= (1<<2) | (0<<1);
    PciWrite16(baseID,PCI_CONFIG_COMMAND,Control);
    kprintf("bar1: 0x%x\n",bar1a.u.port);
    kprintf("ac97: bar0 -> 0x%x. bar1 -> 0x%x. IRQ: 0x%x\n",bar0,bar1,32+irq);
    interrupt_add(32+irq,ac97_irq_handler);
  } else {
    return;
  }
  // Base setup.
  outw(bar1+0x2C,0x3); // device reset.
  outb(bar1+0x1B,(1<<3) | (1<<4)); // enable interrupts.
  //arch_sti();
  outw(bar0,0x1); // reset mixer.
  // Read capabilites.
  controller_caps = inw(bar0+0x28);
  if ((controller_caps & 0x1) == 1) {
    kprintf("ac97: variable sample rate detected.\n");
    outw(bar0+0x2A,0x1);
  }
  // Set maximum volume.
  outw(bar0 + 0x18,0x0);
  ac97_set_output_volume(0x04,0);
  ac97_set_output_volume(0x02,50);
  // Prepare buffers.
  memset(buffers,0,sizeof(Ac97_BufferEntry)*32);
  buffer_phys = (uintptr_t)arch_mmu_getPhysical(buffers);
  for (int i = 0; i < 33; i++) {
    buffersPtr[i] = kmalloc(0x2000);
    memset(buffersPtr[i],0,0x2000);
    buffersPtrPhys[i] = (uintptr_t)arch_mmu_getPhysical(buffersPtr[i]);
  }
  // Registrer in devfs.
  AudioDriver *drv = kmalloc(sizeof(AudioDriver));
  memset(drv,0,sizeof(AudioDriver));
  drv->name = "ac97";
  drv->play = ac97_driver_play;
  drv->stop = ac97_driver_stop;
  drv->setVolume = ac97_driver_setVolume;
  AudioDevice *dev = kmalloc(sizeof(AudioDevice));
  memset(dev,0,sizeof(AudioDevice));
  dev->driver = drv;
  dev->dataQueue = queueSize_create(5*0x1000);
  snd_registerDevice(dev);
  ac97_dev = dev;
  kprintf("AC97 setup done\n");
}

// IRQ.
static uintptr_t ac97_irq_handler(uintptr_t regs) {
  // Read AC97 transfer status register.
  uint16_t status = inb(bar1+0x16);
  if ((status & (1<<3)) == (1<<3)) {
    // clean.
    outb(bar1+0x16,(1<<3));
    uint16_t currBuff = inb(bar1+0x14);
    uint16_t lastValid = ((currBuff+2) & (31));
    if (ac97_dev->dataQueue->size == 0) {
      memset(buffersPtr[lastValid],0,0x1000);
    } else {
      queueSize_dequeue(ac97_dev->dataQueue,buffersPtr[lastValid],0x1000);
    }
   if (ac97_dev->dataQueue->size == 0) {
      memset(buffersPtr[lastValid]+0x1000,0,0x1000);
   } else {
      queueSize_dequeue(ac97_dev->dataQueue,buffersPtr[lastValid]+0x1000,0x1000);
    }
    outb(bar1+0x15,lastValid);
  } else if ((status & (1<<2)) == (1<<2)) {
    outb(bar1+0x16,(1<<2));
    outb(bar1+-0x1B,0);
  } else if ((status & (1<<4)) == (1<<4)) {
    outb(bar1+0x16,(1<<4));
    outb(bar0+0x1B,0);
  }
  return regs;
}
