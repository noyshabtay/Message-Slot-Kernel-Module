#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H
#include <linux/ioctl.h>
#define MAJOR_NUM 240
#define MINOR_LIMIT 256 //there can be at most 256 different message slots device files.
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define BUFFER_LEN 128 //message is up to 128 bytes in write().
#define SUCCESS 0 //desired exit value of the program.
#define FAILURE 1 //exit value of the program in case of failure.
#define DEVICE_NAME "message_slot" //message_slot.ko is the module's name.
#endif