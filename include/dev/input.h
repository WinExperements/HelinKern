#ifndef DEV_INPUT_H
#define DEV_INPUT_H

/* Yeah input file system, required for mouse/keyboard or other stuff */
int input_registerDevice(const char *name);
// Send input data `data` with size `size` to device with ID `inputId`
void input_sendData(int inputId,void *data,int size);
// Unregister input device
void input_unregister(int inputID);
#endif
