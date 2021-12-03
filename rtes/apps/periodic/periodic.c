// periodic.c
// App that takes C, T , cpuid arguments and busy-loops for C μs every T μs on the CPU cpuid. 
// 2021-10-01.

#include <sys/types.h>
#include <linux/sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <linux/unistd.h>
#include <linux/time.h>

#define SIGEXCESS 36

static long C, T;
static struct itimerval timer; // Timer values
void C_timer_init(long period);
void C_timer_handler (int signum);

/**
* @brief Handler for SIGEXCESS interrupt. 
*/
void handlerSigexcess (int signum)
{
    printf ("\nSIGEXCESS!!!\n");
}

/**
* @brief Handler for the C timer interrupt. 
*/
void C_timer_handler (int signum)
{
    // time_t now;
    // time(&now);
    // printf ("Before sleep %s\n", ctime(&now));
    usleep((T-C) * 1000);
    // time(&now);
    // printf ("After sleep %s\n", ctime(&now));
    C_timer_init(C); // restart C timer
}

/**
* @brief Initialize a timer which interrupts every C(completion) seconds, and calls the C_timer_handler at interrupt.
* @param period 
*/
void C_timer_init(long period) {
    /* Install T_timer_handler as the signal handler for SIGVTALRM. */
    signal(SIGALRM, C_timer_handler);

    /* Configure the timer to expire after period microseconds... */
    timer.it_value.tv_sec = period / 1000;
    timer.it_value.tv_usec = (period % 1000) * 1000;

    /* Start a virtual timer. It counts down whenever this process is executing. */
    // printf ("starting again\n");
    setitimer (ITIMER_REAL, &timer, NULL);
}

int main(int argc, char *argv[]) {
    int cpuid;
    unsigned long cpumask;
    
    if (argc != 4)
    {
        printf("Invalid input numbers.\n");
        return 1;
    }

    // Convert command line input into integers.
    C = atoi(argv[1]);
    T = atoi(argv[2]);
    cpuid = atoi(argv[3]);

    // Sanity check
    if(C > T){
        printf("ERROR: C > T\n");
        return -1;
    }
    if(cpuid < 0 || cpuid >3){
        printf("ERROR: cpuid out of range\n");
        return -1;
    }
    if(C > 60000 || T > 60000){
        printf("ERROR: C or T out of range\n");
        return -1;
    }

    cpumask = 1 << cpuid;
    syscall(__NR_sched_setaffinity, getpid(), (size_t)4, &cpumask);
    // printf("Start timers\n");
    signal(SIGEXCESS, handlerSigexcess);
    C_timer_init(C);

    while(1); // busy loop

    return 0;
}