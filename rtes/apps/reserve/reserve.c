// reserve_app
// 2021-10-15

#include <sys/syscall.h>
#include <linux/time.h>
#include <stdio.h>
#include <string.h>


#define __NR_set_reserve 379
#define __NR_cancel_reserve 380
#define MODE_STR_LENGTH 16


void msToTimespec(unsigned long msec, struct timespec *output) {
    output->tv_sec = msec/1000; // from msec to sec
    output->tv_nsec = (msec%1000)*1000000; // the rest to nsec
}

int main(int argc, char *argv[]) {
    int tid, cpuid;
    unsigned long cmsec, tmsec;
    struct timespec C, T;
    char mode[MODE_STR_LENGTH];
    const char setMode[] = "set";
    const char cancelMode[] = "cancel";
    if (argc <= 1) {
        printf("Invalid input numbers.\n");
        return 1;
    }
    printf("set or cancel?\n");
    sscanf(argv[1], "%s", mode);
    printf("mode is %s\n", mode);
    if (!strcmp(mode, setMode)) {
        if (argc != 6)
        {
            printf("Invalid input numbers.\n");
            return 1;
        }
        sscanf(argv[2], "%d", &tid);
        sscanf(argv[3], "%lu", &cmsec);
        sscanf(argv[4], "%lu", &tmsec);
        sscanf(argv[5], "%d", &cpuid);
        printf("Inputs read.\n");
        msToTimespec(cmsec, &C);
        msToTimespec(tmsec, &T);
        // make a syscall
        printf("Ready to set.\n");
        return syscall(__NR_set_reserve, tid, &C, &T, cpuid);
    }
    else if(!strcmp(mode, cancelMode)) {
        if (argc != 3)
        {
            printf("Invalid input numbers.\n");
            return 1;
        }
        sscanf(argv[2], "%d", &tid);
        // make a syscall
        printf("Ready to cancel.\n");
        return syscall(__NR_cancel_reserve, tid);
    }
    return 0;
}