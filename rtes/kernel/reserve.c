/**
 * 2021-10-14
 * This file contains the syscalls to set and cancel thread
 * resouce reservation.
 */

#include <linux/sort.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/signal.h>
#include <asm/siginfo.h>
#include "linux/reserve.h"
#include <asm/div64.h>
#include <linux/cpu.h>
#include "linux/tum.h"


#define SIGEXCESS 36

static const unsigned int ubTable[10] = {1000, 828, 780, 757, 743, 735, 729, 724, 721, 718};
// static const char cpuOnlinePath[]="/sys/devices/system/cpu/cpu%i/online";

static ktime_t cleanupPeriod;
static ktime_t zeroInterval;
static const unsigned long cleanupNs = 100000000; // in ns

static threadReservation_t *reservationList = NULL;
static spinlock_t listLock;
static struct hrtimer cleanupTimer;
// protected by listLock
static volatile int numReservations = 0;
// protected by listLock
// 0 means cpu is online with no reservations
// -1 means cpu is offline
static volatile int numReservationsPerCpu[NUMCPU] = {0 , 0, 0, 0};
// used only for NFTest
static int cur_bin = 0;
static volatile bool bCleanupOnline = false;

// Declare all the helper functions here
static enum hrtimer_restart intrHandlerC(struct hrtimer *pTimer);
static enum hrtimer_restart intrHandlerT(struct hrtimer *pTimer);
static enum hrtimer_restart cleanupHandler(struct hrtimer *pTimer);
bool isTaskAlive(pid_t tid);
bool isTaskRunning(pid_t tid);
ktime_t timespecToKtime(struct timespec input);
threadReservation_t *findReservation(pid_t tid);
threadReservation_t *initReservation(pid_t tid, ktime_t intervalC, ktime_t intervalT, int cpuid);
int insertReservation(threadReservation_t *reservationToInsert);
int removeReservation(threadReservation_t *reservationToRemove);
void updateReservationString(threadReservation_t *currentReservation, unsigned long remainingBudgetNs);
void updateEnergy(threadReservation_t *currentReservation, unsigned long remainingBudgetNs);
int binPacking(pid_t tid, unsigned long C, unsigned long T);
int FFTest(pid_t tid, unsigned long C, unsigned long T);
int NFTest(pid_t tid, unsigned long C, unsigned long T);
int WFTest(pid_t tid, unsigned long C, unsigned long T);
int BFTest(pid_t tid, unsigned long C, unsigned long T);
int LSTTest(pid_t tid, unsigned long C, unsigned long T);
void getUtilSums(unsigned long long * utilSums);
void sortCpus(int *cpuids, unsigned long long *utilSums, int mode);
bool isCpuReservable(pid_t tid, unsigned long C, unsigned long T, int cpuid);
unsigned int ubTest(size_t numToTest, testInfo_t* toTest);
bool rtTest(size_t numToTest, testInfo_t* toTest);
static int testInfoComp(const void *lhs, const void *rhs);
static void testInfoSwap(void *a, void *b, int size);

bool hasActiveReservations(void)
{
    unsigned long listFlags;
    if (!bCleanupOnline)
    {
        return false;
    }
    spin_lock_irqsave(&listLock, listFlags);
    if (numReservations == 0)
    {
        spin_unlock_irqrestore(&listLock, listFlags);
        return false;
    }
    spin_unlock_irqrestore(&listLock, listFlags);
    return true;
}

// mode == 0: max util first
// mode == 1: min util first
void sortCpus(int *cpuids, unsigned long long *utilSums, int mode)
{
    unsigned long long utilSumsTmp[4];
    unsigned long long tmpUtil;
    unsigned int i, j;
    int tmpCpuid;
    memcpy(utilSumsTmp, utilSums, 4 * sizeof(unsigned long long));
    for (i = 0; i < NUMCPU; ++i)
    {
        cpuids[i] = i;
    }
    for (i = NUMCPU; i > 0; --i)
    {
        for (j = 1; j < i; ++j)
        {
            if (mode)
            { // min first
                if (utilSumsTmp[j] < utilSumsTmp[j - 1])
                {
                    tmpUtil = utilSumsTmp[j];
                    utilSumsTmp[j] = utilSumsTmp[j - 1];
                    utilSumsTmp[j - 1] = tmpUtil;
                    tmpCpuid = cpuids[j];
                    cpuids[j] = cpuids[j - 1];
                    cpuids[j - 1] = tmpCpuid;
                }
            }
            else
            { // max first
                if (utilSumsTmp[j] > utilSumsTmp[j - 1])
                {
                    tmpUtil = utilSumsTmp[j];
                    utilSumsTmp[j] = utilSumsTmp[j - 1];
                    utilSumsTmp[j - 1] = tmpUtil;
                    tmpCpuid = cpuids[j];
                    cpuids[j] = cpuids[j - 1];
                    cpuids[j - 1] = tmpCpuid;
                }
            }
            // if ((utilSumsTmp[j] > utilSumsTmp[j - 1]) ^ mode)
            // {
            //     tmpUtil = utilSumsTmp[j];
            //     utilSumsTmp[j] = utilSumsTmp[j - 1];
            //     utilSumsTmp[j - 1] = tmpUtil;
            //     tmpCpuid = cpuids[j];
            //     cpuids[j] = cpuids[j - 1];
            //     cpuids[j - 1] = tmpCpuid;
            // }
        }
    }
    return;
}

// Makes no assumption on the locks
// Assume utilSums is an array of 4 ull
// utilSums: 100.0 -> 1000; 12.5 -> 125
void getUtilSums(unsigned long long * utilSums)
{
    unsigned long listFlags, reservationFlags;
    unsigned long Cns, Tns;
    unsigned long long divTmp;
    unsigned int i;
    int cpuid;
    threadReservation_t *currentReservation;
    for (i=0; i<NUMCPU; ++i)
    {
        utilSums[i] = 0;
    }
    // iterate the list and accumulate for each cpu
    spin_lock_irqsave(&listLock, listFlags);
    for (currentReservation = reservationList;
         currentReservation != NULL;
         currentReservation = currentReservation->next)
    {
        spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
        cpuid = currentReservation->cpuid;
        if (cpuid < 0)
        {
            printk("getUtilSums: broken cpuid = %d, tid = %d", cpuid, (int)currentReservation->tid);
            spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
            continue;
        }
        Cns = ktime_to_ns(currentReservation->intervalC);
        Tns = ktime_to_ns(currentReservation->intervalT);
        divTmp = (unsigned long long)(Cns)*1000ULL;
        do_div(divTmp, Tns);
        utilSums[cpuid] += divTmp;
        spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
    }
    spin_unlock_irqrestore(&listLock, listFlags);

    return;
}

// Makes no assumption on the locks
int FFTest(pid_t tid, unsigned long C, unsigned long T)
{
    unsigned int i;
    unsigned long listFlags;

    spin_lock_irqsave(&listLock, listFlags);
    for (i=0; i<NUMCPU; ++i)
    {
        // check if cpu reservable
        if (numReservationsPerCpu[i] > 0 && isCpuReservable(tid, C, T, i))
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            return i;
        }
    }
    // if no open cpu can fit, look for closed
    for (i=0; i<NUMCPU; ++i)
    {
        // check if cpu closed and available
        if (numReservationsPerCpu[i] == 0)
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            // check if cpu offline, if i is offline, then cpu_up.
            if (!cpu_online(i))
            {
                cpu_up(i);
            }
            return i;
        }
    }
    // no closed found, fail
    spin_unlock_irqrestore(&listLock, listFlags);
    return -1;
}

// Makes no assumption on the locks
int NFTest(pid_t tid, unsigned long C, unsigned long T)
{
    unsigned int i;
    int cpuid;
    unsigned long listFlags;

    spin_lock_irqsave(&listLock, listFlags);
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = (cur_bin + i) % NUMCPU;
        // check if cpu reservable
        if (numReservationsPerCpu[cpuid] > 0 && isCpuReservable(tid, C, T, cpuid))
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            return cpuid;
        }
    }
    // if no open cpu can fit, look for closed
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = (cur_bin + i) % NUMCPU;
        // check if cpu closed and available
        if (numReservationsPerCpu[cpuid] == 0)
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            // check if cpu offline, then cpu_on(cpuid).
            if (!cpu_online(cpuid))
            {
                cpu_up(cpuid);
            }
            return cpuid;
        }
    }
    // no closed found, fail
    spin_unlock_irqrestore(&listLock, listFlags);
    return -1;
}

// Makes no assumption on the locks
int WFTest(pid_t tid, unsigned long C, unsigned long T)
{
    unsigned int i;
    int cpuid;
    unsigned long listFlags;
    unsigned long long utilSums[4];
    int sortedCpu[4];
    getUtilSums(utilSums);
    sortCpus(sortedCpu, utilSums, 1);

    spin_lock_irqsave(&listLock, listFlags);
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = sortedCpu[i];
        // check if cpu reservable
        if (numReservationsPerCpu[cpuid] > 0 && isCpuReservable(tid, C, T, cpuid))
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            return cpuid;
        }
    }
    // if no open cpu can fit, look for closed
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = sortedCpu[i];
        // check if cpu closed and available
        if (numReservationsPerCpu[cpuid] == 0)
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            // check if cpu offline. Turn on cpu if offline
            if (!cpu_online(cpuid))
            {
                cpu_up(cpuid);
            }
            return cpuid;
        }

    }
    // no closed found, fail
    spin_unlock_irqrestore(&listLock, listFlags);
    return -1;
}

// Makes no assumption on the locks
int BFTest(pid_t tid, unsigned long C, unsigned long T)
{
    unsigned int i;
    int cpuid;
    unsigned long listFlags;
    unsigned long long utilSums[4];
    int sortedCpu[4];
    getUtilSums(utilSums);
    sortCpus(sortedCpu, utilSums, 0);

    spin_lock_irqsave(&listLock, listFlags);
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = sortedCpu[i];
        // check if cpu reservable
        if (numReservationsPerCpu[cpuid] > 0 && isCpuReservable(tid, C, T, cpuid))
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            return cpuid;
        }
    }
    // if no open cpu can fit, look for closed
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = sortedCpu[i];
        // check if cpu closed and available
        if (numReservationsPerCpu[cpuid] == 0)
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            // check if cpu offline. If cpu offline, turn on cpu.
            if (!cpu_online(cpuid))
            {
                cpu_up(cpuid);
            }
            return cpuid;
        }
    }
    // no closed found, fail
    spin_unlock_irqrestore(&listLock, listFlags);
    return -1;
}

// Makes no assumption on the locks
int LSTTest(pid_t tid, unsigned long C, unsigned long T)
{
    unsigned int i;
    int cpuid;
    unsigned long listFlags;
    unsigned long long utilSums[4];
    int sortedCpu[4];
    getUtilSums(utilSums);
    sortCpus(sortedCpu, utilSums, 1);

    spin_lock_irqsave(&listLock, listFlags);
    for (i=0; i<NUMCPU; ++i)
    {
        cpuid = sortedCpu[i];
        // make sure every cpu are set onlined before LST check if cpu reservable
        if (isCpuReservable(tid, C, T, cpuid))
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            cur_bin = cpuid;
            // check if cpu offline. If cpu offline, turn on cpu.
            if (!cpu_online(cpuid))
            {
                cpu_up(cpuid);
            }
            return cpuid;
        }
    }
    // no closed found, fail
    spin_unlock_irqrestore(&listLock, listFlags);
    return -1;
}

// check which cpu to fit in
// thing wrapper of the policy check functions
// return cpuid on success, -1 on failure
// Makes no assumption on the locks
int binPacking(pid_t tid, unsigned long C, unsigned long T)
{
    switch (policy)
    {
        case 0:
            return FFTest(tid, C, T);
        case 1:
            return NFTest(tid, C, T);
        case 2:
            return WFTest(tid, C, T);
        case 3:
            return BFTest(tid, C, T);
        case 4:
            return LSTTest(tid, C, T);
        default:
            return FFTest(tid, C, T);
    }

    printk("binPacking reached unexpected state.\n");
    return -1;
}

// perform ub test on an array of testInfo_t
// assume at least can pass one
// return the first index that cannot pass
unsigned int ubTest(size_t numToTest, testInfo_t* toTest)
{
    unsigned long long utilSum;
    unsigned long long divTmp;
    unsigned int i;
    utilSum = 0;
    for (i=0; i<numToTest; ++i)
    {
        divTmp = (unsigned long long)(toTest[i].C)*1000ULL;
        do_div(divTmp, toTest[i].T);
        printk("divTmp = %llu, C = %lu, T = %lu\n", divTmp, toTest[i].C, toTest[i].T);
        utilSum += divTmp;
        printk("i = %u; utilSum = %llu\n", i, utilSum);
        if (utilSum > ubTable[i])
        {
            // we assume toTest[0] can pass
            return i;
        }
    }
    return 0;
}

// perform rt test on an array of testInfo_t
bool rtTest(size_t numToTest, testInfo_t* toTest)
{
    unsigned long long sum = 0, prv = 0, dividend = 0;
    unsigned i;
    // calculate a0
    printk("rtTest\n");
    for (i=0; i<numToTest; ++i)
    {
        sum += toTest[i].C;
    }
    // printk("sum=%llu\n", sum);
    if (sum > toTest[numToTest-1].T)
    {
        printk("RT test a0 failed sum=%llu", sum);
        return false;
    }
    prv = sum;
    while (1)
    {
        // calculate current a from previous a
        sum = 0;
        for (i=0; i<numToTest-1; ++i)
        {
            // tmp += Cs[i]*int((prv+Ts[i]-1)/Ts[i])
            dividend = prv+toTest[i].T-1;
            do_div(dividend, toTest[i].T);
            sum += toTest[i].C * dividend;
        }
        sum += toTest[i].C;
        // printk("sum=%llu\n", sum);
        if (sum > toTest[numToTest-1].T)
        {
            printk("RT test failed sum=%llu\n", sum);
            return false;
        }
        if (prv == sum)
        {
          break;
        }
        prv = sum;
    }
    printk("RT test passed sum=%llu\n", sum);
    return true;
}

// test is a reservation can be done on a cpu
// Assumes the caller has locked the list
bool isCpuReservable(pid_t tid, unsigned long C, unsigned long T, int cpuid)
{
    unsigned int count;
    unsigned int ubTestRet;
    unsigned int i;
    unsigned long reservationFlags;
    testInfo_t* toTest;
    threadReservation_t* currentReservation;

    // malloc an array and put the new one in
    toTest = (testInfo_t*)kmalloc(sizeof(testInfo_t)*(numReservations+1), GFP_KERNEL);
    toTest[0].tid = tid;
    toTest[0].C = C;
    toTest[0].T = T;
    count = 1;
    // iterate the list and populate the array and count the useful
    for (currentReservation = reservationList;
         currentReservation != NULL;
         currentReservation = currentReservation->next)
    {
        spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
        if (currentReservation->cpuid != cpuid || currentReservation->tid == tid)
        {
            spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
            continue;
        }
        toTest[count].tid = currentReservation->tid;
        toTest[count].C = ktime_to_ns(currentReservation->intervalC);
        toTest[count].T = ktime_to_ns(currentReservation->intervalT);
        spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
        ++count;
    }
    // realloc to the actual count
    toTest = (testInfo_t*)krealloc((void *)toTest, sizeof(testInfo_t)*count, GFP_KERNEL);
    sort(toTest, count, sizeof(testInfo_t), &testInfoComp, &testInfoSwap);
    // perform ub tests
    ubTestRet = ubTest(count, toTest);
    if (ubTestRet == 0)
    {
        printk("passed ubTest\n");
        kfree(toTest);
        return true;
    }
    else
    {
        printk("failed ubTest\n");
    }
    // use a for loop for rtTest on tau1 tau2 ...
    for (i=ubTestRet; i<count; ++i)
    {
        if(!rtTest(i+1, toTest))
        {
            printk("TAU%u failed rtTest\n", i);
            kfree(toTest);
            return false;
        }
    }
    kfree(toTest);
    return true;
}

static int testInfoComp(const void *lhs, const void *rhs)
{
    unsigned long lhs_integer = ((const testInfo_t *)lhs)->T;
    unsigned long rhs_integer = ((const testInfo_t *)rhs)->T;

    if (lhs_integer < rhs_integer) return -1;
    if (lhs_integer > rhs_integer) return 1;
    return 0;
}

static void testInfoSwap(void *a, void *b, int size)
{
    testInfo_t t = *(testInfo_t *)a;
    *(testInfo_t *)a = *(testInfo_t *)b;
    *(testInfo_t *)b = t;
}

// thin wrapper for ktime_set
ktime_t timespecToKtime(struct timespec input)
{
    return ktime_set(input.tv_sec, input.tv_nsec);
}

// given tid, find the reservation in reservationList, if cannot find,
// return NULL
// Assumes that the list is locked.
threadReservation_t *findReservation(pid_t tid)
{
    threadReservation_t *currentReservation;
    currentReservation = reservationList;
    while (currentReservation != NULL && currentReservation->tid != tid)
    {
        currentReservation = currentReservation->next;
    }
    return currentReservation;
}

// given a pointer to a reservation, initialize its cpuid, timers, and intervals
// return the newly inserted reservation
// Assumes that the list is locked.
threadReservation_t *initReservation(pid_t tid, ktime_t intervalC, ktime_t intervalT, int cpuid)
{
    threadReservation_t *reservationToInit;
    unsigned long reservationFlags;

    reservationToInit = kmalloc(sizeof(threadReservation_t), GFP_KERNEL);
    reservationToInit->tid = tid;
    spin_lock_init(&reservationToInit->reservationLock);
    spin_lock_irqsave(&reservationToInit->reservationLock, reservationFlags);
    reservationToInit->utilTimestamp = 0;
    reservationToInit->trackEnergy = (bool)energyswitch;
    reservationToInit->energyPerTask = 0;
    reservationToInit->recordCount = 0;
    reservationToInit->strtoprint = (char*) kmalloc(STR_BUFF_SIZE*sizeof(char), GFP_KERNEL);
    reservationToInit->strtoupdate = (char*) kmalloc(STR_BUFF_SIZE*sizeof(char), GFP_KERNEL);
    reservationToInit->strtoprint[0] = '\0';
    reservationToInit->strtoupdate[0] = '\0';
    create_util_file(reservationToInit->tid, &(reservationToInit->fileattr));
    create_energy_folder(reservationToInit->tid, &(reservationToInit->energykobj), &(reservationToInit->energyattr));
    reservationToInit->bAlive = true;
    reservationToInit->cpuid = cpuid;
    reservationToInit->intervalC = intervalC;
    reservationToInit->intervalT = intervalT;
    reservationToInit->remainingBudget = zeroInterval;
    hrtimer_init(&reservationToInit->timerC, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
    hrtimer_init(&reservationToInit->timerT, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
    reservationToInit->timerC.function = &intrHandlerC;
    reservationToInit->timerT.function = &intrHandlerT;
    spin_unlock_irqrestore(&reservationToInit->reservationLock, reservationFlags);
    printk("SYSCALL: initReservation\n");
    return reservationToInit;
}

// given tid, insert the reservation in reservationList,
// return 0 on success, -1 on failure
// Assumes that the list is locked.
int insertReservation(threadReservation_t *reservationToInsert)
{
    unsigned long reservationFlags;
    if (reservationToInsert == NULL)
    {
        return -1;
    }
    // insert to the list head
    reservationToInsert->next = reservationList;
    reservationList = reservationToInsert;
    ++numReservations;

    spin_lock_irqsave(&reservationToInsert->reservationLock, reservationFlags);
    ++numReservationsPerCpu[reservationToInsert->cpuid];
    spin_unlock_irqrestore(&reservationToInsert->reservationLock, reservationFlags);

    printk("SYSCALL: insert to list\n");
    return 0;
}

// given a pointer to a reservation, find the reservation and remove it
// assume the reservation exists
// return 0 on success, -1 on failure
// Assumes that the list is locked.
int removeReservation(threadReservation_t *reservationToRemove)
{
    threadReservation_t *currentReservation;
    unsigned long reservationFlags;
    currentReservation = reservationList;
    while (currentReservation != NULL && currentReservation->next != reservationToRemove)
    {
        currentReservation = currentReservation->next;
    }
    // edge case: removing head
    if (reservationToRemove == reservationList)
    {
        reservationList = reservationList->next;
    }
    else
    {
        currentReservation->next = reservationToRemove->next;
    }
    --numReservations;

    spin_lock_irqsave(&reservationToRemove->reservationLock, reservationFlags);
    --numReservationsPerCpu[reservationToRemove->cpuid];

    hrtimer_cancel(&reservationToRemove->timerC);
    hrtimer_cancel(&reservationToRemove->timerT);

    printk("removing file for %d\n", reservationToRemove->tid);
    remove_util_file(reservationToRemove->tid, &(reservationToRemove->fileattr));
    remove_energy_folder(reservationToRemove->tid, &(reservationToRemove->energykobj), &(reservationToRemove->energyattr));
    kfree(reservationToRemove->strtoprint);
    kfree(reservationToRemove->strtoupdate);
    spin_unlock_irqrestore(&reservationToRemove->reservationLock, reservationFlags);
    kfree(reservationToRemove);
    printk("SYSCALL: removeReservation\n");
    return 0;
}

// given a tid, find the reseravtion and remove it
// assume the tid is valid
// thin wrapper of removeReservation and findReservation
// return 0 on success, -errno on failure
// Makes no assumption on the locks.
int removeReservationByTid(pid_t tid)
{
    threadReservation_t *reservationToRemove;
    unsigned long listFlags;
    // int cpuToDown;
    // cpumask_t cpuMask;
    // mm_segment_t old_fs;
    // loff_t offset = 0;
    // char buf[64] = {0};
    // struct file *cpu_online_fp = NULL;

    spin_lock_irqsave(&listLock, listFlags);
    if ( (reservationToRemove = findReservation(tid)) == NULL )
    {
        spin_unlock_irqrestore(&listLock, listFlags);
        return -EINVAL;
    }
    // cpuToDown = reservationToRemove->cpuid;
    if (removeReservation(reservationToRemove))
    { // fails to remove the reservation
        spin_unlock_irqrestore(&listLock, listFlags);
        return -EFAULT;
    }

    // cpumask_clear(&cpuMask);
    // cpumask_set_cpu(0, &cpuMask);
    // cpumask_set_cpu(1, &cpuMask);
    // cpumask_set_cpu(2, &cpuMask);
    // cpumask_set_cpu(3, &cpuMask);
    // if (sched_setaffinity(tid, &cpuMask))
    // {
    //     printk("removeReservationByTid fail to unbind %d\n", (int)tid);
    //     spin_unlock_irqrestore(&listLock, listFlags);
    //     return 0;
    // }
    // if (numReservationsPerCpu[cpuToDown] == 0 && 
    //     cpuToDown != 0 && cpu_online(cpuToDown))
    // {
    //     printk("removeReservationByTid down cpu%d\n", cpuToDown);
    //     // FIXME cpu_down not working
    //     // cpu_down(cpuToDown);
    //     old_fs = get_fs();
    //     set_fs(KERNEL_DS);
    //     buf[0] = '0';
    //     sprintf(buf, cpuOnlinePath, cpuToDown);
    //     cpu_online_fp = filp_open(buf, O_RDWR, 0);
    //     if ( IS_ERR_OR_NULL(cpu_online_fp) ){
    //         printk(KERN_ALERT "open %s fail\n",buf);
    //         spin_unlock_irqrestore(&listLock, listFlags);
    //         return -1;
    //     }
    //     if(cpu_online_fp->f_op != NULL && cpu_online_fp->f_op->write != NULL) {
    //         cpu_online_fp->f_op->write(cpu_online_fp,"0", 1, &offset);
    //     }
    //     set_fs(old_fs);
    // }

    spin_unlock_irqrestore(&listLock, listFlags);
    return 0;
}

// given a tid, check if a corresponding reseravtion exists
// assume the tid is valid
// thin wrapper of findReservation
// return 1 on success, 0 on failure
// Makes no assumption on the locks.
int doesReservationExist(pid_t tid)
{
    threadReservation_t *reservationToFind;
    unsigned long listFlags;
    spin_lock_irqsave(&listLock, listFlags);
    if ( (reservationToFind = findReservation(tid)) == NULL )
    {
        spin_unlock_irqrestore(&listLock, listFlags);
        return 0;
    }
    spin_unlock_irqrestore(&listLock, listFlags);
    return 1;
}

// iterate the reservation list and output an array
// of cpuNTid and the length of the array
// return 0 on success, -errno on failure
// caller is responsible for kfree the memory
// Makes no assumption on whether the list is initialized.
// Makes no assumption on the locks.
int sendCpuNTid(cpuNTid_t **outInfoArray, int *outNumReservations)
{
    threadReservation_t *currentReservation;
    cpuNTid_t *tmpInfoArray;
    size_t i;
    unsigned long listFlags;
    unsigned long reservationFlags;
    if (!bCleanupOnline) // list is not initialized
    {
        return -EFAULT;
    }
    tmpInfoArray = kmalloc(sizeof(cpuNTid_t)*numReservations, GFP_KERNEL);

    spin_lock_irqsave(&listLock, listFlags);
    for (currentReservation = reservationList, i = 0;
         currentReservation != NULL;
         currentReservation = currentReservation->next, ++i)
    {
        spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
        tmpInfoArray[i].cpuid = currentReservation->cpuid;
        tmpInfoArray[i].tid = currentReservation->tid;
        spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
    }
    spin_unlock_irqrestore(&listLock, listFlags);

    *outNumReservations = numReservations;
    *outInfoArray = tmpInfoArray;

    return 0;
}

void updateEnergy(threadReservation_t *currentReservation, unsigned long remainingBudgetNs)
{
    unsigned long long energyThisPeriod;
    unsigned long Cns;
    // printk("currentReservation->trackEnergy = %d\n", (int)currentReservation->trackEnergy);
    if (!currentReservation->trackEnergy)
    {
        return;
    }
    // accum energy in this period for this thread, ns * mW
    Cns = ktime_to_ns(currentReservation->intervalC);
    energyThisPeriod = ((unsigned long long)(Cns - remainingBudgetNs)) * get_cpu_power();
    currentReservation->energyPerTask += energyThisPeriod;
    // update the total energy
    update_total_energy(energyThisPeriod);
}

/** @brief caller is responsible for the locks */
void updateReservationString(threadReservation_t *currentReservation, unsigned long remainingBudgetNs)
{
    unsigned long long periodInMs;
    unsigned long long utilUsageNs;
    unsigned long Cns;
    unsigned long Tns;
    char newRecord[RECORD_LENGTH];
    char *tmp;

    if (!enabled)
    {
        return;
    }

    ++currentReservation->recordCount;
    Cns = ktime_to_ns(currentReservation->intervalC);
    Tns = ktime_to_ns(currentReservation->intervalT);
    periodInMs = (unsigned long long) Tns;
    do_div(periodInMs, 1000000);
    currentReservation->utilTimestamp += periodInMs;
    utilUsageNs = ((unsigned long long)(Cns - remainingBudgetNs))*1000ULL;
    do_div(utilUsageNs, Tns);
    if(utilUsageNs < 10){
        sprintf(newRecord, "%llu  0.00%llu\n", currentReservation->utilTimestamp, utilUsageNs);
    } else if(utilUsageNs >= 10 && utilUsageNs < 100){
        sprintf(newRecord, "%llu  0.0%llu\n", currentReservation->utilTimestamp, utilUsageNs);
    } else if(utilUsageNs >= 100){
        sprintf(newRecord, "%llu  0.%llu\n", currentReservation->utilTimestamp, utilUsageNs);
    }

    if (currentReservation->recordCount < MAX_RECORD_COUNT)
    {
        strncat(currentReservation->strtoprint, newRecord, RECORD_LENGTH);
    }
    else
    {
        // switch the two buffers
        currentReservation->recordCount = 1;
        tmp = currentReservation->strtoprint;
        currentReservation->strtoprint = currentReservation->strtoupdate;
        currentReservation->strtoupdate = tmp;
        currentReservation->strtoupdate[0] = '\0';
        strncat(currentReservation->strtoprint, newRecord, RECORD_LENGTH);
    }
}

// called when context switch into a task
// given a tid, find the reseravtion and activate it
// assume the tid is valid
// return 0 on success, -errno on failure
// Makes no assumption on the locks.
int tryActivateReservation(pid_t tid)
{
    threadReservation_t *reservationToActivate;
    unsigned long listFlags;
    unsigned long reservationFlags;

    if (!bCleanupOnline)
    {
        return -EFAULT;
    }

    spin_lock_irqsave(&listLock, listFlags);

    if ( (reservationToActivate = findReservation(tid)) == NULL )
    {
        spin_unlock_irqrestore(&listLock, listFlags);
        return -EINVAL;
    }

    spin_lock_irqsave(&reservationToActivate->reservationLock, reservationFlags);

    if (!hrtimer_active(&reservationToActivate->timerT))
    {
        hrtimer_start(&reservationToActivate->timerT, reservationToActivate->intervalT, HRTIMER_MODE_REL_PINNED);
    }

    // restart the C timer with the remaining budget
    if (hrtimer_active(&reservationToActivate->timerC))
    {
        // printk("WARNING: Activating a running clock!\n");
    }
    else
    {
        if (ktime_to_ns(reservationToActivate->remainingBudget) > 0)
        {
            // printk("Reactivate with remaining budget %lld\n", ktime_to_ns(reservationToActivate->remainingBudget));
            hrtimer_start(&reservationToActivate->timerC, reservationToActivate->remainingBudget, HRTIMER_MODE_REL_PINNED);
            reservationToActivate->remainingBudget = zeroInterval;
        }
        else
        {
            // printk("WARNING: remaining budget %lld\n", ktime_to_ns(reservationToActivate->remainingBudget));
        }
    }
    spin_unlock_irqrestore(&reservationToActivate->reservationLock, reservationFlags);
    spin_unlock_irqrestore(&listLock, listFlags);

    return 0;
}

// called when context switch out of a task
// given a tid, find the reseravtion and deactivate it
// assume the tid is valid
// return 0 on success, -errno on failure
// Makes no assumption on the locks.
int tryDeactivateReservation(pid_t tid)
{
    threadReservation_t *reservationToDeactivate;
    unsigned long listFlags, reservationFlags;

    if (!bCleanupOnline)
    {
        return -EFAULT;
    }

    spin_lock_irqsave(&listLock, listFlags);

    if ( (reservationToDeactivate = findReservation(tid)) == NULL )
    {
        spin_unlock_irqrestore(&listLock, listFlags);
        return -EINVAL;
    }

    spin_lock_irqsave(&reservationToDeactivate->reservationLock, reservationFlags);

    if (hrtimer_active(&reservationToDeactivate->timerC))
    {
        // cancel the C timer and record the remaining budget
        reservationToDeactivate->remainingBudget = hrtimer_get_remaining(&reservationToDeactivate->timerC);
        hrtimer_cancel(&reservationToDeactivate->timerC);
        // printk("Deactivating with remaining budget %lld\n", ktime_to_ns(reservationToDeactivate->remainingBudget));
    }
    else
    {
        // printk("WARNING: Deactivating a non-running clock!\n");
        // reservationToDeactivate->remainingBudget = zeroInterval;
    }

    spin_unlock_irqrestore(&reservationToDeactivate->reservationLock, reservationFlags);
    spin_unlock_irqrestore(&listLock, listFlags);

    return 0;
}

/** @brief check if thread is still alive*/
bool isTaskAlive(pid_t tid)
{
    rcu_read_lock();
    if (find_task_by_vpid(tid))
    {
        rcu_read_unlock();
        return true;        
    }
    rcu_read_unlock();
    return false;
}

/** @brief check if thread is still running*/
bool isTaskRunning(pid_t tid)
{
    struct task_struct* currentTask;
    rcu_read_lock();
    if ((currentTask = find_task_by_vpid(tid)))
    {
        if (currentTask->state == TASK_RUNNING)
        {
            rcu_read_unlock();
            return true;        
        }
    }
    rcu_read_unlock();
    return false;
}

/** @brief set task to TASK_UNINTERRUPTIBLE*/
bool setTaskUninterruptible(pid_t tid)
{
    struct task_struct* currentTask;
    rcu_read_lock();
    if ((currentTask = find_task_by_vpid(tid)))
    {
        if (currentTask->state != TASK_UNINTERRUPTIBLE)
        {
            currentTask->state = TASK_UNINTERRUPTIBLE;
            set_tsk_need_resched(currentTask); // this will trigger context switch
            rcu_read_unlock();
            return true;        
        }
    }
    else
    {
        printk("setTaskUninterruptible: Nonexist task!\n");
    }
    rcu_read_unlock();
    return false;
}

/** @brief set task to TASK_RUNNING*/
bool setTaskRunning(pid_t tid)
{
    struct task_struct* currentTask;
    rcu_read_lock();
    if ((currentTask = find_task_by_vpid(tid)))
    {
        if (currentTask->state != TASK_RUNNING)
        {
            wake_up_process(currentTask); // this will trigger context switch
            rcu_read_unlock();
            return true;        
        }
    }
    else
    {
        printk("setTaskRunning: Nonexist task!\n");
    }
    rcu_read_unlock();
    return false;
}

static enum hrtimer_restart intrHandlerC(struct hrtimer *pTimer)
{
    unsigned long reservationFlags;
    threadReservation_t *currentReservation;

    currentReservation = container_of(pTimer, threadReservation_t, timerC);

    spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
    // check if thread is still alive
    if (!isTaskAlive(currentReservation->tid))
    {
        printk("C handler caught dead thread %u\n", (unsigned int)currentReservation->tid);
        printk("currentReservation->bAlive = %u\n", (unsigned int)currentReservation->bAlive);
        currentReservation->bAlive = false;
        spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
        return HRTIMER_NORESTART; // do not run bugdet timer for a dead thread
    }
    // send signal to user
    // rcu_read_lock();
    // send_sig(SIGEXCESS, find_task_by_vpid(currentReservation->tid), 1);
    // rcu_read_unlock();
    // instead of sending signals, we now put the process to sleep
    if(setTaskUninterruptible(currentReservation->tid))
    {
        printk("C handler put %u to sleep\n", (unsigned int)currentReservation->tid);
    }
    else
    {
        printk("C handler failed to put %u to sleep\n", (unsigned int)currentReservation->tid);
    }

    printk("%u out of budget\n", (unsigned int)currentReservation->tid);
    spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
    // should start from next T instead of now
    return HRTIMER_NORESTART;
}

static enum hrtimer_restart intrHandlerT(struct hrtimer *pTimer)
{
    unsigned long reservationFlags;
    threadReservation_t *currentReservation;
    unsigned long remainingBudgetNs;

    currentReservation = container_of(pTimer, threadReservation_t, timerT);

    spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
    printk("%u end of period\n", (unsigned int)currentReservation->tid);

    hrtimer_forward_now(pTimer, currentReservation->intervalT);
    // check if thread is still alive
    if (!isTaskAlive(currentReservation->tid))
    {
        printk("T handler caught dead thread %u\n", (unsigned int)currentReservation->tid);
        printk("currentReservation->bAlive = %u\n", (unsigned int)currentReservation->bAlive);
        currentReservation->bAlive = false;
        hrtimer_cancel(&currentReservation->timerC);
        spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
        return HRTIMER_NORESTART; // do not run period timer for a dead thread
    }

    // if thread not active now, should we only replenish budget, don't start budget timer
    if (hrtimer_active(&currentReservation->timerC))
    {
        remainingBudgetNs = ktime_to_ns(hrtimer_get_remaining(&currentReservation->timerC));
        updateReservationString(currentReservation, remainingBudgetNs);
        updateEnergy(currentReservation, remainingBudgetNs);
        if (!isTaskRunning(currentReservation->tid))
        {
            printk("ERROR: T handler caught C timer running for non-running task!\n");
        }
        // if active, restart C timer with full budget
        currentReservation->remainingBudget = zeroInterval;
        hrtimer_cancel(&currentReservation->timerC);
        hrtimer_start(&currentReservation->timerC, currentReservation->intervalC, HRTIMER_MODE_REL_PINNED);
    }
    else
    {
        remainingBudgetNs = ktime_to_ns(currentReservation->remainingBudget);
        updateReservationString(currentReservation, remainingBudgetNs);
        updateEnergy(currentReservation, remainingBudgetNs);
        currentReservation->remainingBudget = currentReservation->intervalC;
        if (isTaskRunning(currentReservation->tid))
        {
            // wait for context switch
            printk("T handler wait for context switch %u\n", (unsigned int)currentReservation->tid);
        }
        else
        {
            printk("T handler set %d to running\n", (unsigned int)currentReservation->tid);
            if(setTaskRunning(currentReservation->tid))
            {
                printk("T handler wake up %u\n", (unsigned int)currentReservation->tid);
            }
            else
            {
                printk("T handler failed to wake up %u\n", (unsigned int)currentReservation->tid);
            }
        }
    }
    spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
    return HRTIMER_RESTART;
}

/**
 * @brief Iterates reservation list and remove the dead reservations.
 */
static enum hrtimer_restart cleanupHandler(struct hrtimer *pTimer)
{
    unsigned long listFlags;
    unsigned long reservationFlags;
    threadReservation_t *currentReservation;
    threadReservation_t *reservationToRemove;

    spin_lock_irqsave(&listLock, listFlags);

    currentReservation = reservationList;
    while (currentReservation != NULL)
    {
        spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
        // check if thread is still alive
        if (!currentReservation->bAlive)
        {
            printk("remove dead reservation %u\n", (unsigned int)currentReservation->tid);
            reservationToRemove = currentReservation;
            currentReservation = currentReservation->next;
            // release reservationLock for removeReservation
            spin_unlock_irqrestore(&reservationToRemove->reservationLock, reservationFlags);
            removeReservation(reservationToRemove);
        }
        else
        {
            spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
            currentReservation = currentReservation->next;
        }
    }

    spin_unlock_irqrestore(&listLock, listFlags);

    hrtimer_forward_now(pTimer, cleanupPeriod);
    return HRTIMER_RESTART;
}

/**
 * @param tid, thread ID to set reservation for;
 *             if 0, the call should apply to the calling thread
 * @param C, completion time, expected on the order of hundres of
 *           microseconds
 * @param T, period, expected on the order of hundres of microseconds
 * @param cpuid, the CPU ID to which the thread should be pinned,
 *               valid range {0,1,2,3}
 * @return 0 if there is no error
 *         negative errno error code if there is error
 */
asmlinkage int set_reserve(pid_t tid, struct timespec *C,
                           struct timespec *T, int cpuid)
{
    threadReservation_t *currentReservation;
    struct timespec kC, kT;
    unsigned long listFlags, reservationFlags;
    unsigned long Cns, Tns;
    cpumask_t cpuMask;

    if (!bCleanupOnline)
    {
        spin_lock_init(&listLock);
        cleanupPeriod = ktime_set(0, cleanupNs);
        zeroInterval = ktime_set(0, 0);
    }

    // sanity check
    if (tid < 0 || !C || !T || cpuid > 3)
    {
        printk("set_reserve fail on invalid input values.\n");
        return -EINVAL;
    }
    // check if the thread exists
    if (!isTaskAlive(tid))
    {
        printk("set_reserve fail on non-existing tid.\n");
        return -EINVAL; // do not run period timer for a dead thread
    }
    // C and T are user pointers, need to copy to kernel level
    if ( copy_from_user(&kC, C, sizeof(struct timespec)) || 
         copy_from_user(&kT, T, sizeof(struct timespec)) )
    { // copy_from_user return 0 on success
        printk("set_reserve fail to copy C or T.\n");
        return -EFAULT;
    }
    // C cannot be larger than T
    Cns = (unsigned long)timespec_to_ns(&kC);
    Tns = (unsigned long)timespec_to_ns(&kT);
    if ( Cns > Tns )
    {
        printk("set_reserve C > T.\n");
        return -EFAULT;
    }
    // check if reservable
    if (cpuid >= 0)
    {
        spin_lock_irqsave(&listLock, listFlags);
        if (!isCpuReservable(tid, Cns, Tns, cpuid))
        {
            spin_unlock_irqrestore(&listLock, listFlags);
            printk("set_reserve selected cpu is not reservable.\n");
            return EBUSY;
        }
        if (!cpu_online(cpuid))
        {
            printk("set_reserve up cpu %d.\n", cpuid);
            cpu_up(cpuid);
        }
        spin_unlock_irqrestore(&listLock, listFlags);
    }
    else if (cpuid == -1)
    {
        // perform bin packing
        // bin packing should be able to turn on cpus
        cpuid = binPacking(tid, Cns, Tns);
        if (cpuid == -1)
        {
            printk("set_reserve binPacking failed.\n");
            return EBUSY;
        }
        printk("set_reserve set on CPU%d.\n", cpuid);
    }
    else
    {
        printk("set_reserve fail on invalid cpuid.\n");
        return -EINVAL;
    }
    // bind the thread to the cpu
    cpumask_clear(&cpuMask);
    cpumask_set_cpu(cpuid, &cpuMask);
    if (sched_setaffinity(tid, &cpuMask) < 0)
    {
        printk("set_reserve fail to bind %u to cpu %u\n", (unsigned int)tid, (unsigned int)cpuid);
        return -EFAULT;
    }

    if (removeReservationByTid(tid) == -EFAULT)
    {
        printk("Fail to remove old reservation\n");
        return -EFAULT;
    }
    spin_lock_irqsave(&listLock, listFlags);
    // initialize reservation
    currentReservation = initReservation(tid, timespecToKtime(kC), timespecToKtime(kT), cpuid);

    spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);

    currentReservation->remainingBudget = currentReservation->intervalC;
    printk("BUDGET is %llu\n", ktime_to_ns(currentReservation->intervalC));

    spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);

    // NOTE insert after timer start to prevent unwanted read from context swtich
    insertReservation(currentReservation);

    if (!bCleanupOnline)
    {
        hrtimer_init(&cleanupTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        cleanupTimer.function = &cleanupHandler;
        hrtimer_start(&cleanupTimer, cleanupPeriod, HRTIMER_MODE_REL);
        bCleanupOnline = true;
    }
    spin_unlock_irqrestore(&listLock, listFlags);
    if (isTaskRunning(tid))
    {
        tryActivateReservation(tid);
    }
    return 0;
}

/**
 * @param tid, thread ID of whose reservation is to be cancelled
 *   If the tid argument is 0, the call should apply to the calling thread. 
 * @return 0 if there is no error, negative errono error code id there is error
 */
asmlinkage int cancel_reserve(pid_t tid)
{
    // sanity check
    if (tid < 0)
    {
        return -EINVAL;
    }
    printk("SYSCALL_cancel_reserve called on %u\n", (unsigned int)tid);
    return removeReservationByTid(tid);
}
