#ifndef DEV_INPUT_H
#define DEV_INPUT_H

/* 
 * Input file system.
 *
 * Doesn't really an FS, but can help to register an input device in devfs
 * Your input device must be an dev_t structure to register it
 * There fucntions just helps to register input device
*/

/* Just returns an available device name in devfs
 * Returned format is "input<available ID>"
*/
char *input_getFreeName();
#endif
