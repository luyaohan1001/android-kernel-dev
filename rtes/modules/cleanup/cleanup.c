// cleanup_module
// 2021-10-04

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h> // module_param
#include <linux/unistd.h> // sys_call_table

#include <linux/sched.h> // current
#include <linux/fs.h> // file struct
#include <linux/fdtable.h>
#include <linux/fs_struct.h>
#include <linux/dcache.h> // d_path

#include <asm/uaccess.h> // strncpy_from_user

extern void *sys_call_table[];

static char* comm;
module_param(comm, charp, 0644);

asmlinkage long (*original_exit) (int); // original_exit

asmlinkage int our_sys_exit(int error_code)
{
    int i;
    char pathBuf[128];
    char *path;
    struct files_struct *current_files; 
    struct fdtable *files_table;
    struct path files_path;
    // Check this process name has substring comm
    if(strstr(current->comm, comm)) // if null then not found
    {
        // Print process name and PID
        printk("cleanup: process '%s' (PID %d) did not close files:\n", current->comm, current->pid);
        // Get the files
        current_files = current->files;
        files_table = files_fdtable(current_files);
        // Iterate the file_table and print the path
        i = 0;
        while(files_table->fd[i] != NULL) { 
            files_path = files_table->fd[i]->f_path;
            path = d_path(&files_path, pathBuf, 128*sizeof(char));
            printk("cleanup: %s\n", path);
            ++i;
        }
    } 
    // Call the original sys_exit
    return original_exit(error_code);
}

// override the original syscall when loading the module
int init_module()
{
    original_exit = sys_call_table[__NR_exit_group];
    sys_call_table[__NR_exit_group] = our_sys_exit;
    return 0;
}

// restore the original syscall when unloading the module
void cleanup_module()
{
    sys_call_table[__NR_exit_group] = original_exit;
}

MODULE_LICENSE("CMU18648_2021FALL_TEAM11");