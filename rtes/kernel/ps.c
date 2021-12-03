// C file for count_rt_threads and list_rt_threads 
// 2021-10-01.
// Reference: https://docs.huihoo.com/doxygen/linux/kernel/3.7/structtask__struct.html

#include <linux/kernel.h>
#include <asm/uaccess.h> // put_user
#include <linux/sched.h> // for_each_process

typedef struct TaskStats {
    unsigned tgid; // pid
    unsigned pid;  // tid
    unsigned rt_priority; //  priority
} TaskStats_t;

typedef struct TaskStruct {
    TaskStats_t taskStats; // direct copyables
    char * comm; // name 
} TaskStruct_t;
    
asmlinkage long count_rt_threads(int* ptrNumThread) {
    long ret;
    int count = 0; // number of real-time threads
    struct task_struct * crtTask; 
    for_each_process(crtTask) {
        if (crtTask->rt_priority > 0) { // real-time process
            count ++;
        }
    }
    ret = put_user(count,ptrNumThread);
    return ret;
}

asmlinkage long list_rt_threads(int numThread, void* usrPtrTaskStruct) {
    TaskStats_t tempTaskStats;
    TaskStruct_t* usrPrtCasted = (TaskStruct_t*)usrPtrTaskStruct;
    struct task_struct * crtTask; 
    int i = 0;
    for_each_process(crtTask) {
        if (crtTask->rt_priority > 0) { // real-time process
            tempTaskStats.tgid = crtTask->tgid;
            tempTaskStats.pid = crtTask->pid;
            tempTaskStats.rt_priority = crtTask->rt_priority;
            copy_to_user(&(usrPrtCasted[i].taskStats), &tempTaskStats, sizeof(TaskStats_t));
            copy_to_user(usrPrtCasted[i].comm, crtTask->comm, TASK_COMM_LEN);
            ++i;
        }
    }
}