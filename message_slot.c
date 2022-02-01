#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include "message_slot.h"
#include <linux/slab.h>
//includes as in the recitation:
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/

//==================== DECLARATIONS ===========================
MODULE_LICENSE("GPL");

typedef struct channel_data {
	char* message;
	size_t message_len;
    unsigned int channel_num;
    struct channel_data* next;
} channel_data;

typedef struct device_interface {
	unsigned int minor;
    //uses indirection from file_data's struct.
	char** message;
	size_t* message_len;
} device_interface;

channel_data* devices[MINOR_LIMIT];

static int device_open(struct inode*, struct file*);
static int device_release(struct inode*, struct file*);
static ssize_t device_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t device_write(struct file*, const char __user*, size_t, loff_t*);
static long device_ioctl(struct file*, unsigned int, unsigned long);
static int __init simple_init(void);
static void __exit simple_cleanup(void);
void init_devices(void);
void clean_devices(void);
channel_data* create_channel(unsigned int, unsigned int);
channel_data* find_channel_or_create_it(unsigned int, unsigned int);
channel_data* find_channel(unsigned int, unsigned int);

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode* inode,
                        struct file*  file) {
    device_interface* dev;
    printk("Invoking device_open(%p)\n", file);
    dev = kmalloc(sizeof(device_interface), GFP_KERNEL);
    if (dev == NULL) {
        printk("device's file kmalloc() failed.\n");
        return -ENOMEM;
    }
    dev->message = NULL;
    dev->message_len = NULL;
    dev->minor = iminor(inode);
    file->private_data = (void*)dev;
    printk("device_open(%p) succeeded.\n", file);
    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release(struct inode* inode,
                            struct file*  file) {
    device_interface* dev = (device_interface*)(file->private_data);
    printk("Invoking device_release(%p, %p)\n", inode, file);
    kfree(dev);
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset) { 
    device_interface* dev = (device_interface*)(file -> private_data);
    char** message = dev->message;
    char* the_message;
    size_t message_len;
    int copy_to_user_returned_val;
    printk("Invocing device_read(%p,%ld).\n", file, length);

    if (message == NULL || buffer == NULL) {
        return -EINVAL;
    }
    the_message = *message;
    message_len = *(dev->message_len);
    if (the_message == NULL || message_len == 0) {
        return -EWOULDBLOCK;
    }
    if (length < message_len) {
        return -ENOSPC;
    }
    printk("read input verification passed successfully.\n");

    printk("reads meassage: %s\n", the_message);
    copy_to_user_returned_val = copy_to_user(buffer, the_message, message_len);
    if (copy_to_user_returned_val == SUCCESS) { //only 0 indicates success for atomicity.
        return message_len;
    }
    printk("failed to read %d bytes.\n", copy_to_user_returned_val);
    return -EFAULT;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file*       file,
                            const char __user* buffer,
                            size_t             length,
                            loff_t*            offset) {
    device_interface* dev = (device_interface*)(file -> private_data);
    char ** message = dev->message;
    char* curr_message;
    char* next_message;
    int copy_from_user_returned_val;
    printk("Invoking device_write(%p,%ld).\n", file, length);

    if (message == NULL || buffer == NULL) {
        return -EINVAL;
    }
    if (length == 0 || BUFFER_LEN < length) {
        return -EMSGSIZE;
    }
    printk("write input verification passed successfully.\n");

    curr_message = *message;
    next_message = kmalloc(length, GFP_KERNEL);
    if(next_message == NULL) {
        printk("new message kmalloc() failed.\n");
        return -ENOMEM;
    }

    copy_from_user_returned_val = copy_from_user(next_message, buffer, length);
    printk("new message written: %s\n", next_message);
    if (copy_from_user_returned_val == SUCCESS) { //only 0 indicates success for atomicity.
        if (curr_message != NULL) {
            kfree(curr_message);
        }
        *(dev -> message) = next_message;
        *(dev -> message_len) = length;
        return length;
    }
    printk("failed to write %d bytes.\n", copy_from_user_returned_val);
    kfree(next_message);
    return -EFAULT;
}

//----------------------------------------------------------------
static long device_ioctl(struct file* file,
                        unsigned int ioctl_command_id,
                        unsigned long ioctl_param) {
    device_interface* dev;
    channel_data* curr_channel;
    printk("Invoking ioctl with param %ld.\n", ioctl_param);

    if(MSG_SLOT_CHANNEL != ioctl_command_id || ioctl_param == 0) {
        return -EINVAL;
    }
    printk("ioctl input verification passed successfully.\n");

    dev = file->private_data;
    curr_channel = find_channel_or_create_it(dev->minor, ioctl_param);
    if (curr_channel == NULL) {
        printk("find_channel_or_create_it() failed.\n");
        return -ENOMEM;
    }
    dev->message = &(curr_channel->message);
    dev->message_len = &(curr_channel->message_len);
    printk("device_open() succeeded - channel %ld was added.\n", ioctl_param);
    return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
{
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void) {
    int rc = -1;
    // init dev struct
    rc = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);
    if(rc < 0) {
        printk(KERN_ERR "%s registraion failed for %d major number.\n", DEVICE_NAME, MAJOR_NUM);
        return rc;
    }
    init_devices();
    printk("Message_slot module initialized successfully.\n");
    return SUCCESS;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void) {
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    clean_devices();
    printk("Message_slot module unregistered cleanly.\n");
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//===================== INNER FUNCTIONS =========================

void init_devices(void) {
    int i;
    for(i=0; i<MINOR_LIMIT; i++) {
        devices[i] = NULL;
    }
}

void clean_devices(void) {
    int i;
    channel_data* curr_channel;
    channel_data* next_channel;

    for(i=0; i<MINOR_LIMIT; i++) { //for each device.
        curr_channel = devices[i];
        while(curr_channel != NULL) { //for each channel in the device.
            next_channel = curr_channel->next;
            kfree(curr_channel);
            curr_channel = next_channel;
        }
    }
}

channel_data* create_channel(unsigned int minor, unsigned int channel) {
    channel_data* curr_channel = devices[minor];
    channel_data* new_channel = (channel_data*)kmalloc(sizeof(channel_data), GFP_KERNEL);
    
    if (new_channel == NULL) {
        printk("new channel kmalloc() failed.\n");
        return new_channel;
    }
    new_channel->message = NULL;
    new_channel->message_len = 0;
    new_channel->channel_num = channel;
    new_channel->next = curr_channel;
    devices[minor] = new_channel;
    return new_channel;
}

channel_data* find_channel_or_create_it(unsigned int minor, unsigned int channel_number) {
    channel_data* curr_channel = find_channel(minor, channel_number);
    if(curr_channel == NULL) {
        curr_channel = create_channel(minor, channel_number);
    }
    return curr_channel;
}

channel_data* find_channel(unsigned int minor, unsigned int channel_number) {
    channel_data* curr_channel = devices[minor];
    while(curr_channel != NULL) {
        if (curr_channel->channel_num == channel_number) {
            return curr_channel;
        }
        curr_channel = curr_channel->next;
    }
    return NULL;
}