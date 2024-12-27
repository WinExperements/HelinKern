#pragma once
#include <typedefs.h>
#include <lib/fifosize.h>
#define SND_IOCTL_PLAY		10
#define SND_IOCTL_PAUSE		12
#define SND_IOCTL_STOP		14
#define SND_IOCTL_SAMPLE	16
#define SND_IOCTL_DEVINFO	20
#define SND_IOCTL_SET_VOLUME	13
#define SND_VOL_OUTPUT_SPEAKER	2
struct _audio_device;
typedef struct _audio_driver {
  char *name;   // name of device driver.
  bool (*play)(struct _audio_device *device);
  bool (*pause)();
  bool (*stop)();
  bool (*setSampleRate)(int dev,int rate);
  int (*setVolume)(int forWhat,int vol);
} AudioDriver;

typedef struct _audio_device {
  AudioDriver *driver;  // pointer to device driver.
  queueSize *dataQueue;
  dev_t *newDevice;	// Initialized by sound subsystem. DON'T Initialize IT IN YOUR DRIVER!
  /* Global per-device status! */
  bool playing;
} AudioDevice;

void snd_init();
void snd_registerDevice(AudioDevice *device);

