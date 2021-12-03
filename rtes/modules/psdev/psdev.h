#ifndef PSDEV_H
#define PSDEV_H

#include <linux/module.h>
#include <linux/kernel.h>
// #include <linux/device.h> // for create devices
#include <linux/kdev_t.h> // dev_t
#include <linux/fs.h> // alloc_chrdev_region, unregister_chrdev_region
#include <linux/sched.h> // for_each_process, task_struct
#include <asm/uaccess.h> // put_user

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "psdev"
#define BUF_LEN 512

#endif // PSDEV_H