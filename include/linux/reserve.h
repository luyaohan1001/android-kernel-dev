/*
 * 2021-10-23
 * This file is the header of the syscalls to set and cancel thread
 * resouce reservation.
 * Put under include/linux to make sched.c aware.
 */
#ifndef __RESERVE__H
#define __RESERVE__H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kobject.h>

#define STR_BUFF_SIZE 1024
#define RECORD_LENGTH 32
#define MAX_RECORD_COUNT 32
#define NUMCPU 4

typedef struct threadReservation
{
    /** 
     * @brief If thread is dead, this will be set as false. A timer
     *        will remove dead reservations periodically.
     */
    bool bAlive;
    /** 
     * @brief Protect the reservation from being modified by multiple
     *        interrupt handlers.
     */
    spinlock_t reservationLock;
    /** 
     * @brief Remaining budget before context switch.
     */
    ktime_t remainingBudget;

    struct kobject *energykobj;
    struct kobj_attribute energyattr;
    struct kobj_attribute fileattr;
    unsigned long long utilTimestamp;
    bool trackEnergy;
    unsigned long long energyPerTask; // in ns * mW
    int recordCount;
    char* strtoprint;
    char* strtoupdate;
    // NOTE Be careful! Possible buffer overflow here.
    pid_t tid;
    int cpuid;
    ktime_t intervalC;
    ktime_t intervalT;
    struct hrtimer timerC;
    struct hrtimer timerT;
    struct threadReservation *next;
} threadReservation_t;

typedef struct cpuNTid
{
    int cpuid;
    pid_t tid;
} cpuNTid_t;

typedef struct testInfo
{
    pid_t tid;
    unsigned long C;
    unsigned long T;
} testInfo_t;

int sendCpuNTid(cpuNTid_t **outInfoArray, int *outNumReservations);
int tryActivateReservation(pid_t tid);
int tryDeactivateReservation(pid_t tid);
int removeReservationByTid(pid_t tid);
int doesReservationExist(pid_t tid);
bool hasActiveReservations(void);

#endif
