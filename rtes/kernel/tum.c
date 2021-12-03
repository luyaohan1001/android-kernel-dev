
#include "linux/tum.h"
#include "linux/cpufreq.h"
#include "linux/cpumask.h"
#include "linux/reserve.h"


#define DEFAULT_STRN_LENGTH 128
static ssize_t enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t enabled_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);
static ssize_t energyswitch_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t energyswitch_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);

static ssize_t filepid_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t filepid_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t reserves_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t reserves_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);
static ssize_t partition_policy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t partition_policy_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);

static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t freq_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);
static ssize_t power_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t power_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);
static ssize_t energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t energy_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count);
static ssize_t single_pid_energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t single_pid_energy_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

// folders
static struct kobject *rtes_kobject;
static struct kobject *taskmon_kobject;
static struct kobject *config_kobject;
static struct kobject *tasks_kobject;
struct kobject *util_kobject;

spinlock_t energyLock;
const char energy_str[] = "energy";
int enabled = 0;
int energyswitch = 0;
int policy = 0;
unsigned long long cpu_freq = 0;
unsigned long long total_energy = 0; // ns * mW
static struct kobj_attribute enabled_attribute = __ATTR(enabled, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), enabled_show, enabled_store); //give all read and write premissions 0666
static struct kobj_attribute energyswitch_attribute = __ATTR(energy, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), energyswitch_show, energyswitch_store);
//static struct kobj_attribute filepid_attribute = __ATTR(NULL, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), filepid_show, filepid_store);
static struct kobj_attribute reserves_attribute = __ATTR(reserves, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), reserves_show, reserves_store);
static struct kobj_attribute partition_policy_attribute = __ATTR(partition_policy, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), partition_policy_show, partition_policy_store);


// files
static struct kobj_attribute freq_attribute = __ATTR(freq, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), freq_show, freq_store);
static struct kobj_attribute energy_attribute = __ATTR(energy, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), energy_show, energy_store);
static struct kobj_attribute power_attribute = __ATTR(power, (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH), power_show, power_store);

unsigned long long freq2power(unsigned long long cpu_freq);


void update_cpu_freq(void)
{
    unsigned int i;
    unsigned long long cpu_freq_tmp;
    /*
        * Iterate through all the cpu and get the frequency(kHz) of online cpu.
        */
    for (i = 0; i < NUMCPU; ++i) { 
        if (cpu_online(i)) {    
            cpu_freq_tmp = (unsigned long long)cpufreq_quick_get(i); // Get freq in kHz
            do_div(cpu_freq_tmp, 1000); // Convert to MHz
            cpu_freq = cpu_freq_tmp;
            printk("cpu %u is online. The cpu freq is: %llu\n", i, cpu_freq);
            break;
        }
    }
}

unsigned long long get_cpu_power(void)
{
    update_cpu_freq();
    return freq2power(cpu_freq);
}

void update_total_energy(unsigned long long input)
{
    unsigned long energyFlags;
    printk("ADD %llu to total energy\n", input);
    spin_lock_irqsave(&energyLock, energyFlags);
    total_energy += input;
    spin_unlock_irqrestore(&energyLock, energyFlags);
}

/**
 * @brief The callback function for reading the enable file     
 */
static ssize_t enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", enabled);
}

/**
 * @brief The callback function for writing the enable file     
 */
static ssize_t enabled_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    sscanf(buf, "%d", &enabled);
    return count;
}

/**
 * @brief The callback function for reading the enable file     
 */
static ssize_t energyswitch_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", energyswitch);
}

/**
 * @brief The callback function for writing the enable file     
 */
static ssize_t energyswitch_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    sscanf(buf, "%d", &energyswitch);
    return count;
}

/**
 * @brief The callback function for reading the util file     
 */
static ssize_t filepid_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
    // long utilization_fraction;
    int ret;
    unsigned long reservationFlags;
    // char printdummy[]  = "0 0.25\n4 0.25\n8 0.25\n12 0.25\n16 0.25\n";
    threadReservation_t *currentReservation;
    currentReservation = container_of(attr, threadReservation_t, fileattr);
    spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
    // printk("%s", currentReservation->strtoprint);
    ret = sprintf(buf, "%s", currentReservation->strtoprint);
    currentReservation->strtoprint[0] = '\0';
    spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
    return ret;
}

/**
 * @brief The callback function for writing the util file
 */
static ssize_t filepid_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) 
{
    return count;
}

//2.2.1
/**
 * @brief The callback function for reading the reserve file     
 */
static ssize_t reserves_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    cpuNTid_t * outInfoArray;
    int outNumReservations;
    int i;
    struct task_struct *task_list;
    char outstring[256]; //each line has about 30 char, assume 
    int numWrittenBytes = 0;
    sendCpuNTid(&outInfoArray, &outNumReservations);
    numWrittenBytes += sprintf(outstring + numWrittenBytes,"TID\tPID\tPRIO\tCPU\tNAME\n");
    for (i = 0; i < outNumReservations; i++)
    {
        rcu_read_lock();
        for_each_process(task_list) 
        {
        if (task_list->pid == outInfoArray[i].tid)
            {
                numWrittenBytes += sprintf(outstring + numWrittenBytes,"%d\t%d\t%d\t%d\t%s\n",task_list->pid,task_list->tgid,task_list->rt_priority,outInfoArray[i].cpuid,task_list->comm);
            }
        }
        rcu_read_unlock();
    }
    kfree(outInfoArray);
    return sprintf(buf, "%s", outstring);    
}

/**
 * @brief The callback function for writing the reserve file     
 */
static ssize_t reserves_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    return count;
}

//2.2.3
/**
 * @brief The callback function for reading the reserve file     
 */
static ssize_t partition_policy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (policy == 0)
    {
        return sprintf(buf, "FF\n");
    }
    else if (policy == 1)
    {
        return sprintf(buf, "NF\n");
    }
    else if (policy == 2)
    {
        return sprintf(buf, "WF\n");
    }
    else if (policy == 3)
    {
        return sprintf(buf, "BF\n");
    }
    else if (policy == 4)
    {
        return sprintf(buf, "LST\n");
    }
    else
    {
        return sprintf(buf, "FF\n");
    }
    
}

/**
 * @brief The callback function for writing the reserve file     
 */
static ssize_t partition_policy_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    // check if any active reservation exists, assuming no current active reservation
    char policy_name[5];
    if (hasActiveReservations())
    {
        printk("hasActiveReservations\n");
        return EBUSY;
    }
    sscanf(buf, "%s", policy_name);
    if (policy_name[0] == 'F')
    {
        policy = 0;
    }
    else if (policy_name[0] == 'N')
    {
        policy = 1;
    }
    else if (policy_name[0] == 'W')
    {
        policy = 2;
    }
    else if (policy_name[0] == 'B')
    {
        policy = 3;
    }
    else if (policy_name[0] == 'L')
    {
        policy = 4;
    }
    else
    {
        policy = 0;
    }

    return count;
}



    
/**
 * @brief Given a cpu frequency, return the power of cpu. 
 * 
 *     k = 0.00442 mW/MHz
 *     a = 1.67
 *     B = 25.72 mW
 *
 *    |    Frequency (MHz) |    (k * f ^ a) + B (mW)|
 *    |         51                     |  28.8609748821535     |
 *    |         102                    |  35.7150404532436     |
 *    |         204                    |  57.5256773486462     |
 *    |         340                    |  100.363511295784     |
 *    |         475                    |  156.186976336381     |
 *    |         640                    |  240.375898007833     |
 *    |         760                    |  311.729835670755     |
 *    |         860                    |  377.308517402599     |
 *    |        1000                    |  478.015502588093     |
 *    |        1100                    |  556.052269279532     |
 *    |        1200                    |  638.994560979516     |
 *    |        1300                    |  726.70329371034      |
 *
 */
unsigned long long freq2power(unsigned long long cpu_freq) { // mW
    switch (cpu_freq) {
        case 51: // 51 MHz 
            return 28;
        case 102:
            return 35;
        case 204:
            return 57;
        case 340:
            return 100;
        case 475:
            return 156;
        case 640: 
            return 240;
        case 760:
            return 311;
        case 860:
            return 377;
        case 1000:
            return 478;
        case 1100:
            return 556;
        case 1200:
            return 638;
        case 1300:
            return 726;
        default:
            printk("Error in freq2power(). Unknown freq %llu.\n", cpu_freq);
            return 726;
    }
}

/**
 * @brief The callback function for reading the power (total) file
 */
static ssize_t power_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    update_cpu_freq();
    return sprintf(buf, "%llu\n", freq2power(cpu_freq));
}

/**
 * @brief The callback function for writing power (total) file
 */
static ssize_t power_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    return count;
}


/**
 * @brief The callback function for reading the energy (total) file.
 */
static ssize_t energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    unsigned long long energy_to_show;
    energy_to_show = total_energy;
    printk("total energy in ns*mW = %llu\n", energy_to_show);
    // change the unit from ns*mW to mJ
    do_div(energy_to_show, 1000000000);
    return sprintf(buf, "%llu\n", energy_to_show);
}

/**
 * @brief The callback function for writing the reserve file     
 */
static ssize_t energy_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    unsigned long energyFlags;
    // write any value will trigger reset
    spin_lock_irqsave(&energyLock, energyFlags);
    total_energy = 0;
    spin_unlock_irqrestore(&energyLock, energyFlags);
    printk("total_energy reset\n");
    return count;
}


/**
 * @brief The callback function for reading the energy file for a single pid.
 * @return Energy in mJ
 */
static ssize_t single_pid_energy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

    threadReservation_t *currentReservation;
    unsigned long long energy_nsmW;
    unsigned long reservationFlags;
    currentReservation = container_of(attr, threadReservation_t, energyattr);

    spin_lock_irqsave(&currentReservation->reservationLock, reservationFlags);
    // get the energy for this thread from reserve.c
    energy_nsmW = currentReservation->energyPerTask; // C in ns*mW
    spin_unlock_irqrestore(&currentReservation->reservationLock, reservationFlags);
    do_div(energy_nsmW, 1000000000); // convert to mJ
    return sprintf(buf, "%llu\n", energy_nsmW);
}

/**
 * @brief The callback function for writing the reserve file     
 */
static ssize_t single_pid_energy_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    return count;
}

/**
 * @brief The callback function for reading the reserve file     
 */
static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    update_cpu_freq();
    return sprintf(buf, "%llu\n", cpu_freq);
}

/**
 * @brief The callback function for writing the freq file     
 */
static ssize_t freq_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    return count;
}

/**
 * @brief create a util file with given pid in sys/rtes/taskmon/util
 * @param pid_to_create, filename for the file to be created
 * @return 0 if there is no error
 *         error if sysfs_create_file failed
 */
int create_util_file(pid_t pid_to_create, struct kobj_attribute* filepid_attribute)
{
    int error;
    char* filepid; 
 
    filepid = (char*) kmalloc(sizeof(char)*32, GFP_KERNEL); //32 should be enough
    sprintf(filepid, "%d", pid_to_create);

    printk("filename pid is  = %s\n", filepid);
    filepid_attribute->attr.name = filepid;
    filepid_attribute->attr.mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    filepid_attribute->show = filepid_show;
    filepid_attribute->store = filepid_store;

    error = sysfs_create_file(util_kobject, &filepid_attribute->attr);
    if (error) 
    {
        printk("failed to create the %s file in /sys/rtes/taskmon/util \n",filepid_attribute->attr.name);
    }
    else
    {
        printk("created the %s file in /sys/rtes/taskmon/util \n",filepid_attribute->attr.name);
    }
    
    return error;
}
/**
 * @brief remove a util file with given pid in sys/rtes/taskmon/util
 * @param pid_to_remove, filename for the file to be removed
 * @return 0 sysfs_remove_file is a void
 *         
 */

int remove_util_file(pid_t pid_to_remove, struct kobj_attribute* filepid_attribute)
{
    // struct attribute attr;
    // char *filepid;
    // filepid = (char*) kmalloc(sizeof(char)*32, GFP_KERNEL);
    // sprintf(filepid, "%d", pid_to_remove);
    // attr.name = filepid;
    // attr.mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    sysfs_remove_file(util_kobject, &filepid_attribute->attr);
    // kfree(filepid);
    kfree(filepid_attribute->attr.name);
    printk("File %d has been removed \n", pid_to_remove);
    // kfree(filepid);
    return 0;
}





// How to get the utilization? container_of -> find current reservation -> find util

/**
 * @brief create pid folder that contains an energy file with given pid.
 * @param pid_to_create, filename for the folder to be created
 * @param folderobj, pointer to pointer of `the kobject (a folder) created.
 * @return 0 if there is no error
 *         error if sysfs_create_file failed
 */
int create_energy_folder(pid_t pid_to_create, struct kobject** folderobj, struct kobj_attribute* file_energy_attribute) {
    int error;

        // convert pid from pid_t to string
        // char* folderpid; 
    // folderpid = (char*) kmalloc(sizeof(char)*32, GFP_KERNEL); //32 should be enough
    char folderpid[32];
    sprintf(folderpid, "%d", pid_to_create);
    printk("foldername pid is  = %s\n", folderpid);

        // <pid> folder creation/
    *folderobj = kobject_create_and_add(folderpid, tasks_kobject);
        // If creation has failed.
    if(!(*folderobj)) 
    {
        return -ENOMEM;
    }


    // File creation 
    file_energy_attribute->attr.name = energy_str;
    file_energy_attribute->attr.mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    file_energy_attribute->show = single_pid_energy_show;
    file_energy_attribute->store = single_pid_energy_store;

    error = sysfs_create_file((*folderobj), &(file_energy_attribute->attr));
    if (error) 
    {
        printk("failed to create the %s file in /sys/rtes/tasks/%s/ \n", file_energy_attribute->attr.name, folderpid);
    }
    else
    {
        printk("created the %s file in /sys/rtes/tasks/%s/ \n", file_energy_attribute->attr.name, folderpid);
    }
    return error;
}



/**
 * @brief remove a pid folder and its energy file with given pid in sys/rtes/taskmon/util
 * @param pid_to_remove, filename for the folder to be removed
 * @param handler of the kobject (a file) to be deleted.
 * @return 0 sysfs_remove_file is a void
 *         
 */

int remove_energy_folder(pid_t pid_to_remove, struct kobject** folderobj, struct kobj_attribute* file_energy_attribute) {
        // struct attribute attr;
    // char *filepid;
        // filepid = (char*) kmalloc(sizeof(char)*32, GFP_KERNEL);
        // sprintf(filepid, "%d", pid_to_remove);
        // attr.name = filepid;
    // attr.mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        
    sysfs_remove_file((*folderobj), &(file_energy_attribute->attr));
    kobject_put((*folderobj));
    printk("Energy file %d has been removed \n", pid_to_remove);
    return 0;
}


/**
 * @brief create the file path and enabled file with a device initcall
 * @param pid_to_create, filename for the file to be removed
 * @return 0 sysfs_remove_file is a void
 *         
 */
static int __init createfiles_init (void)
{
    int error;

    spin_lock_init(&energyLock);
    //create file path sys/rtes/taskmon
    rtes_kobject = kobject_create_and_add("rtes", NULL);
    if(!rtes_kobject)
    {
        return -ENOMEM;
    }

    taskmon_kobject = kobject_create_and_add("taskmon", rtes_kobject);
    if(!taskmon_kobject) 
    {
        kobject_put(rtes_kobject);
        return -ENOMEM;
    }

    //create file enabled
    error = sysfs_create_file(taskmon_kobject, &enabled_attribute.attr);
    if (error) 
    {
     printk("failed to create the enabled file in /sys/rtes/taskmon \n");
    }
    //create util folder 
    util_kobject = kobject_create_and_add("util", taskmon_kobject);
    if(!util_kobject)
    {
        kobject_put(rtes_kobject);
        kobject_put(taskmon_kobject);
        return -ENOMEM;
    }
    // Lab 3
    //2.2.1 create file reserves
    error = sysfs_create_file(rtes_kobject, &reserves_attribute.attr);
    if (error) 
    {
     printk("failed to create the reserves file in /sys/rtes/ \n");
    }
    //2.2.3 create file partition_policy
    error = sysfs_create_file(rtes_kobject, &partition_policy_attribute.attr);
    if (error) 
    {
     printk("failed to create the partition_policy file in /sys/rtes/ \n");
    }
    //2.3.1
    //create energy enable switch
    //create folders first
    config_kobject = kobject_create_and_add("config", rtes_kobject);
    if(!config_kobject) 
    {
        kobject_put(rtes_kobject);
        return -ENOMEM;
    }

    tasks_kobject = kobject_create_and_add("tasks", rtes_kobject);
    if(!tasks_kobject) 
    {
        kobject_put(rtes_kobject);
        return -ENOMEM;
    }
    //create the switch file
    error = sysfs_create_file(config_kobject, &energyswitch_attribute.attr);
    if (error) 
    {
     printk("failed to create the energyswitch file in /sys/rtes/config \n");
    }
    //create data files
    //freq
    error = sysfs_create_file(rtes_kobject, &freq_attribute.attr);
    if (error) 
    {
     printk("failed to create the freq file in /sys/rtes/ \n");
    }
    //power
    error = sysfs_create_file(rtes_kobject, &power_attribute.attr);
    if (error) 
    {
     printk("failed to create the power file in /sys/rtes/ \n");
    }
    //energy
    error = sysfs_create_file(rtes_kobject, &energy_attribute.attr);
    if (error) 
    {
     printk("failed to create the energy file in /sys/rtes/ \n");
    }
    //each task's energy
    // }
    //printk("Trying device_initcall\n");
    //Lab3
    
    return 0;
}

// static void __exit createfiles_exit (void)
// {
//     printk("Module uninitialized successfully \n");
//     kobject_put(rtes_kobject);
//     kobject_put(taskmon_kobject);
//     kobject_put(util_kobject);
// }


device_initcall(createfiles_init);
