// sloppyapp_app
// 2021-10-04

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char buff[128];
    FILE* fp;
    printf("This is a sloppy app!\n");
    fp = fopen("sloppyfile.txt", "w+");   
    fprintf(fp, "test");
    // printf("File left unclosed!\n");
    fclose(fp);
    return 0;
}
