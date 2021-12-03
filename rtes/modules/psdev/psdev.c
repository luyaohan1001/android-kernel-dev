// psdev_module
// 2021-10-01
// reference: https://tldp.org/LDP/lkmpg/2.6/html/x569.html

#include "psdev.h"

#define EOF -1

// Global variables are declared as static, so are global within the file. 
static int Major; // Major number assigned to our device driver
static int Device_Open = 0; // Is device open?  
                            // Used to prevent multiple access to device
static char msg[BUF_LEN]; // The msg the device will give when asked
static char *msg_Ptr;
static struct file_operations fops =
{
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

int init_module (void)
{
    // register a major num for the device
    Major = register_chrdev(0, DEVICE_NAME, &fops);

    if (Major < 0)
    {
      printk(KERN_ALERT "Registering char device failed with %d\n", Major);
      return Major;
    }

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
    printk(KERN_INFO "Remove the device file and module when done.\n");

    return SUCCESS;
}

void cleanup_module(void)
{
    // unregister the device
    // unregister_chrdev does not return value anymore
    unregister_chrdev(Major, DEVICE_NAME);
}


// Implementation of file operations

// open the device for cat
static int device_open(struct inode *inode, struct file *file)
{
    int msg_Curser = 0;
    struct task_struct *task_list;
    // cannot open a device multiple times
    if (Device_Open) return EBUSY;
    Device_Open++; // increase the counter
    // prepare the msg
    // field headings of the table
    msg_Curser += sprintf(msg, "tid\tpid\tpr\tname\n");
    for_each_process(task_list) {
        if (task_list->rt_priority == 0) continue;
        msg_Curser  += sprintf(msg + msg_Curser, "%d\t%d\t%d\t%s\n",
                               task_list->pid, task_list->tgid,
                               task_list->rt_priority, task_list->comm);
    }
    msg_Ptr = msg;
    // increase the module usage counter
    try_module_get(THIS_MODULE);
    return SUCCESS;
}

// release device after cat finishes
static int device_release(struct inode *inode, struct file *file)
{
    Device_Open--; // ready for the next cat
    // decrease the module usage counter
    module_put(THIS_MODULE);
    return 0;
}

// use for_each_process to get process info
static ssize_t device_read(struct file *filp, // type defined in include/linux/fs.h
                           char *buffer, // ptr to user space buffer
                           size_t length, // length of the buffer
                           loff_t * offset)
{ 
    int bytes_read = 0; // Number of bytes successfully written to user's buffer
    // Check if we have already read msg once
    // NOTE Cat will keep reading until it read 0 bytes
    if (*msg_Ptr == 0) return 0; 
    // Copy data to the user buffer 
    while (length && *msg_Ptr)
    {
        // use put_user to copy data from kernel space to user space
        put_user(*(msg_Ptr++), buffer++);
        --length;
        ++bytes_read;
    }
    put_user(EOF, buffer++); // add end of file
    ++bytes_read;
    // return number of bytes successfully read
    return bytes_read;
}

// write is not supported for psdev
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    return ENOTSUPP;
}
