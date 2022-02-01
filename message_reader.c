#include "message_slot.h"
#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]);

int main(int argc, char *argv[]) {
  int file_desc;
  int ret_val;
  size_t message_len;
  char buffer[BUFFER_LEN];

  if(argc != 3) {
    perror("3 arguments were expected but there weren't exactly 3 of them");
    exit(FAILURE);
  }
  //opens device.
  file_desc = open(argv[1], O_RDONLY);
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
  //reads the message.
  message_len = read(file_desc, buffer, BUFFER_LEN);
  if (message_len < 0) {
    perror("read() failed");
    exit(FAILURE);
  }

  close(file_desc);
  //writes the message to STDOUT.
  ret_val = write(1, buffer, message_len); //File Descriptor 1 is reserved for STDOUT.
  if (ret_val != message_len) {
    perror("write() failed");
  }
  return SUCCESS;
}