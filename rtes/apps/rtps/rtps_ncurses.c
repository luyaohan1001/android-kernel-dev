// rtps_app
// 2021-10-04

#include <linux/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>

#define TASK_COMM_LEN 16
#define __NR_count_rt_threads 377
#define __NR_list_rt_threads 378

#define PAIR_BW       1
#define BRIGHT_WHITE  15

typedef struct TaskStats {
    unsigned tgid; // pid
    unsigned pid;  // tid
    unsigned rt_priority; //  priority
} TaskStats_t;

typedef struct TaskStruct {
    TaskStats_t taskStats; // direct copyables
    char * comm; // name 
} TaskStruct_t;


/* returns the current second in a minute, 0~59*/
uint8_t get_second(){
    time_t t;       // This is a type suitable for storing the calendar time.
    struct tm *timeinfo;  // This is a structure used to hold the time and date.
    time(&t);   // Calculates the current calender time and encodes it into time_t format.
    timeinfo = localtime(&t); // The value of timer is broken up into the structure tm.
    return timeinfo->tm_sec;
}

cmpfunc(const void* a, const void* b)
{
   return ((((TaskStruct_t*)b)->taskStats).rt_priority - (((TaskStruct_t*)a)->taskStats).rt_priority);
}

int main(void) {
    unsigned int i;
    unsigned char last_sec = 0;
    unsigned char curr_sec = 0;
    int numRtThreads;
    TaskStruct_t* listRtThreads;
    last_sec = get_second();
    

    while(1) {
        curr_sec = get_second();
        if ( (curr_sec - last_sec) == 2) {

            initscr();
            clear();
            refresh();

            last_sec = curr_sec;
            numRtThreads = 0;
            syscall(__NR_count_rt_threads, &numRtThreads);
            // malloc struct array and struct members 
            listRtThreads = malloc(sizeof(TaskStruct_t)*numRtThreads);
            for (i = 0; i < numRtThreads; ++i)
            {
                listRtThreads[i].comm = malloc(sizeof(char)*TASK_COMM_LEN);
            }
            syscall(__NR_list_rt_threads, numRtThreads, listRtThreads);
            // sort the array
            qsort(listRtThreads, numRtThreads, sizeof(TaskStruct_t), cmpfunc);
            
            // print all the threads
            clear();    
            printw("Second: %d\ntid\tpid\tpr\tname\n",curr_sec);    /* Print */
            refresh();
            for (i = 0; i < numRtThreads; ++i)
            {
                printw("%d\t%d\t%d\t%s\n",
                        listRtThreads[i].taskStats.pid,
                        listRtThreads[i].taskStats.tgid,
                        listRtThreads[i].taskStats.rt_priority,
                        listRtThreads[i].comm);
            }
            refresh();            /* Print it on to the real screen */
            clear();
            
            // free struct members and struct array
            for (i = 0; i < numRtThreads; ++i)
            {
                free(listRtThreads[i].comm);
            }
            free(listRtThreads);

        } 
    
    
    }

    clear();
    refresh();
    endwin();            
    return 0;

}



