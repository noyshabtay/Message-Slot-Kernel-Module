#include "message_slot.h"
#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]);

int main(int argc, char* argv[]) {
  int file_desc;
  int ret_val;
  size_t message_len;
  
  if(argc != 4) {
    perror("4 arguments were expected but there weren't exactly 4 of them");
    exit(FAILURE);
  }
  //opens device.
  file_desc = open(argv[1], O_WRONLY);
  if (file_desc < 0) {
    perror("open() failed");
    exit(FAILURE);
  }
  //opens the desired channel in device.
  ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, atoi(argv[2]));
  if (ret_val < 0) {
    perror("ioctl() failed");
    exit(FAILURE);
  }
  //writes the message to the channel.
  message_len = strlen(argv[3]);
  ret_val = write(file_desc, argv[3], message_len);
  if (ret_val != message_len) {
    perror("write() failed");
    exit(FAILURE);
  }

  close(file_desc);
  return SUCCESS;
}