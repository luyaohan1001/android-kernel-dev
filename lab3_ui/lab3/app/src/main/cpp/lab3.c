// calc_module
// 2021-10-01.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <linux/time.h>
#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <dirent.h>

#define __NR_calc 376
#define __NR_set_reserve 379
#define __NR_cancel_reserve 380

#define __NR_write 64
#define LOG_TAG "BELLPEPPER"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

JNIEXPORT jint JNICALL Java_com_example_lab2_MainActivity_findNumUtilFiles (JNIEnv* env, jobject instance) {
    // char PATH[] = "/data/rtes/utils_data/";

    char PATH[] = "/sys/rtes/taskmon/util/";
    struct dirent *entry;
    // counts the total number of util files.
    int util_files_cnt = 0;
    DIR *util_files_dir = opendir(PATH);

    // Iterate through all util_files_cnt in the directory.
    while( (entry = readdir(util_files_dir) )) {
        // Iterate through all util_files_cnt in the util_files_dir path.
        // Local index to load content_single_file into content_single_file_arr
        if (entry->d_name[0] != '.') {
            util_files_cnt++;
        }
    }
    closedir(util_files_dir);


    return util_files_cnt;
}


JNIEXPORT jobjectArray JNICALL Java_com_example_lab2_MainActivity_readUtilsFilesName (JNIEnv* env, jobject instance, jint numOfUtilFiles) {
    // char PATH[] = "/data/rtes/utils_data/";
    char PATH[] = "/sys/rtes/taskmon/util/";

    FILE* read_file;
    struct dirent *entry;
    // counts the total number of util files.
    int util_files_cnt = 0;
    char single_file_name [65536];
    DIR *util_files_dir = opendir(PATH);

    // Define an object (String type) array.
    jobjectArray file_name_arr;
    file_name_arr = (jobjectArray) (*env)->NewObjectArray(env, numOfUtilFiles, (*env)->FindClass(env, "java/lang/String"), NULL);
    char single_file_name_arr[numOfUtilFiles] ;
    LOGD("numOfUtilFiles is -- %d", numOfUtilFiles);
    // Iterate through all util_files_cnt in the directory.
    int cnt = 0;
    while( (entry = readdir(util_files_dir) )) {
        // Iterate through all util_files_cnt in the util_files_dir path.
        // Local index to load content_single_file into content_single_file_arr

        if (entry->d_name[0] != '.') {
            LOGD("File %3d: %s\n", util_files_cnt, entry->d_name);
            jchar file_path[1024];
            // Ex: /data/rtes/utils_data/1209
            strcpy(file_path, PATH);
            strcat(file_path, entry->d_name);
            strcat(single_file_name, entry->d_name);
            LOGD("Opening file: %s", file_path);
            // single_file_name_arr[cnt] = single_file_name;
            (*env)->SetObjectArrayElement(env, file_name_arr, cnt, (*env)->NewStringUTF(env, entry->d_name));
            cnt++;
        }
    }
    closedir(util_files_dir);

    return file_name_arr;
}



JNIEXPORT void JNICALL Java_com_example_lab2_MainActivity_startMonitoring (JNIEnv* env, jobject instance) {
    FILE *fp;
    char PATH[] = "/sys/rtes/taskmon/enabled";
    // char PATH[] = "/data/rtes/utils_data/1000";
    fp = fopen(PATH, "w");
    // ascii of 1 is 49
    fputc(49,fp);
    fclose(fp);
}



JNIEXPORT void JNICALL Java_com_example_lab2_MainActivity_stopMonitoring (JNIEnv* env, jobject instance) {
    FILE *fp;
    char PATH[] = "/sys/rtes/taskmon/enabled";
    // char PATH[] = "/data/rtes/utils_data/1000";
    fp = fopen(PATH, "w");
    // ascii of 0 is 48
    fputc(48,fp);
    fclose(fp);
}

JNIEXPORT jstring JNICALL Java_com_example_lab2_MainActivity_readUtilsFilesContent (JNIEnv* env, jobject instance, jstring file) {
    //char PATH[] = "/sys/rtes/taskmon/";
    // char PATH[] = "/data/rtes/utils_data/";
    char PATH[] = "/sys/rtes/taskmon/util/";
    FILE* read_file;
    struct dirent *entry;
    // counts the total number of util files.
    jint util_files_cnt = 0;
    char content_single_file[512] ;

    // Iterate through all util_files_cnt in the directory.

    jchar file_path[256];
    // Ex: /data/rtes/utils_data/1209
    strcpy(file_path, PATH);
    char* file_parse = (*env)->GetStringUTFChars(env, file, 0);
    strcat(file_path, file_parse);
    LOGD("Opening file: %s", file_path);
    read_file = fopen(file_path, "r");
    char buffer[1024]={0};
    while(fread(buffer, sizeof(char),1024,read_file)!=0){
        LOGD("content:%s",buffer);
    }


    jstring ret = (*env)->NewStringUTF(env, buffer);
    return ret;
}



JNIEXPORT jstring JNICALL Java_com_example_lab2_MainActivity_readFreqFile (JNIEnv* env, jobject instance) {
    char PATH[] = "/sys/rtes/freq";
    FILE* read_file;
    struct dirent *entry;
    // counts the total number of util files.
    jint util_files_cnt = 0;
    char content_single_file[512] ;

    // Iterate through all util_files_cnt in the directory.

    jchar file_path[256];
    // Ex: /data/rtes/utils_data/1209
    strcpy(file_path, PATH);
    LOGD("Opening file: %s", file_path);
    read_file = fopen(file_path, "r");
    char buffer[1024]={0};
    while(fread(buffer, sizeof(char),1024,read_file)!=0){
        LOGD("content:%s",buffer);
    }
    jstring ret = (*env)->NewStringUTF(env, buffer);
    return ret;
}

JNIEXPORT jstring JNICALL Java_com_example_lab2_MainActivity_readPowerFile (JNIEnv* env, jobject instance) {
    char PATH[] = "/sys/rtes/power";
    FILE* read_file;
    struct dirent *entry;
    // counts the total number of util files.
    jint util_files_cnt = 0;
    char content_single_file[512] ;

    // Iterate through all util_files_cnt in the directory.

    jchar file_path[256];
    // Ex: /data/rtes/utils_data/1209
    strcpy(file_path, PATH);
    LOGD("Opening file: %s", file_path);
    read_file = fopen(file_path, "r");
    char buffer[1024]={0};
    while(fread(buffer, sizeof(char),1024,read_file)!=0){
        LOGD("content:%s",buffer);
    }
    jstring ret = (*env)->NewStringUTF(env, buffer);
    return ret;
}

JNIEXPORT jstring JNICALL Java_com_example_lab2_MainActivity_readEnergyFile (JNIEnv* env, jobject instance) {
    char PATH[] = "/sys/rtes/energy";
    FILE* read_file;
    struct dirent *entry;
    // counts the total number of util files.
    jint util_files_cnt = 0;
    char content_single_file[512] ;

    // Iterate through all util_files_cnt in the directory.

    jchar file_path[256];
    // Ex: /data/rtes/utils_data/1209
    strcpy(file_path, PATH);
    LOGD("Opening file: %s", file_path);
    read_file = fopen(file_path, "r");
    char buffer[1024]={0};
    while(fread(buffer, sizeof(char),1024,read_file)!=0){
        LOGD("content:%s",buffer);
    }
    jstring ret = (*env)->NewStringUTF(env, buffer);
    return ret;
}

JNIEXPORT void JNICALL Java_com_example_lab2_MainActivity_startTaskReserve (JNIEnv* env, jobject instance, jint threadID, jint C, jint T, jint cpuID) {
    struct timespec output_C;
    output_C.tv_sec = C/1000; // from msec to sec
    output_C.tv_nsec = (C%1000)*1000000; // the rest to nsec


    struct timespec output_T;
    output_T.tv_sec = T/1000; // from msec to sec
    output_T.tv_nsec = (T%1000)*1000000; // the rest to nsec
    syscall(__NR_set_reserve,  threadID, &output_C , &output_T, cpuID);
}



JNIEXPORT void JNICALL Java_com_example_lab2_MainActivity_cancelTaskReserve (JNIEnv* env, jobject instance, jint budgetID) {
    syscall(__NR_cancel_reserve, budgetID);
}
JNIEXPORT jint JNICALL Java_com_example_lab2_MainActivity_add (JNIEnv *env, jobject instance, jint a, jint b) {
    // Tag is "LUYAO"
    LOGD("hello from jni call in lab2.c!\n");
    char in1[64] = "1";
    char in2[64] = "2";

    sprintf(in1, "%d", a);
    sprintf(in2, "%d", b);


    jchar result_ptr[50];
    syscall(__NR_calc, &in1, &in2, '+', (char*)result_ptr);
    LOGD("%s\n", result_ptr);
    return (jint)(atoi(result_ptr));

}

