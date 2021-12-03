#ifndef __TUM__H
#define __TUM__H

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h> 

//adapted from https://pradheepshrinivasan.github.io/2015/07/02/Creating-an-simple-sysfs/
int create_util_file (pid_t pid_to_create, struct kobj_attribute* filepid_attribute);
int remove_util_file (pid_t pid_to_remove, struct kobj_attribute* filepid_attribute);


int create_energy_folder(pid_t pid_to_create, struct kobject** folderobj, struct kobj_attribute* file_energy_attribute);
int remove_energy_folder(pid_t pid_to_remove, struct kobject** folderobj, struct kobj_attribute* file_energy_attribute);

void update_total_energy(unsigned long long input);
unsigned long long get_cpu_power(void);

extern int enabled;
extern int energyswitch;

extern int policy;
#endif 
