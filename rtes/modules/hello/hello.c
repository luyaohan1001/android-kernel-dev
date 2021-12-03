// hello_word_module
// 2021-09-29

#include <linux/module.h>
#include <linux/kernel.h>

int init_module(void)
{
    printk("Hello, world! Kernel-space -- the land of the free and the home of the brave.\n");
    return 0;
}

void cleanup_module(void)
{
    // deliberately left blank
}