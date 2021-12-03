// calc_module
// 2021-10-01.

#include <sys/syscall.h>
#include <stdio.h>

#define __NR_calc 376


int main(int argc, char *argv[]) {
    char result_ptr[50];
    if (argc != 4)
    {
        printf("Invalid input numbers.\n");
        return 1;
    }
    // make a syscall
    syscall(__NR_calc, argv[1], argv[3], *argv[2], result_ptr);
    printf("%s\n",result_ptr);
    return 0;
}