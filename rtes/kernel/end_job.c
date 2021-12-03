#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/reserve.h>



/**
 * @brief syscall to end current job
 */
asmlinkage long end_job(void){

    // Check if the reservation exists
    if (!doesReservationExist(current->pid))
    {
        // failed to deactivate
        printk("SYSCALL: end_job cannot find reservation\n");
        return -1;
    }
    // If already suspended, no need to end again
    if (current->state != TASK_UNINTERRUPTIBLE)
    {
        printk("SYSCALL: end_job put current to sleep\n");
        // // NOTE make sure the c timer is turned off!
        // tryDeactivateReservation(current->pid);
        set_current_state(TASK_UNINTERRUPTIBLE);
        set_tsk_need_resched(current); // this will trigger context switch
    }


    return 0;
}
