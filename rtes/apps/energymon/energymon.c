#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <linux/rtc.h>
#include <time.h>
#include <unistd.h>


#define DEFAULT_LEN 256

void int_handler() {
    exit(0);
}


int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: ./energymon tid\n");
	}

	signal(SIGINT, int_handler);
	char freq_path[DEFAULT_LEN];
	char power_path[DEFAULT_LEN];
	char energy_path[DEFAULT_LEN];

	char freq[DEFAULT_LEN];
	char power[DEFAULT_LEN];
	char energy[DEFAULT_LEN];
	char system_cmd[DEFAULT_LEN];
	unsigned int pid = atoi(argv[1]);
		
	FILE *fp_freq, *fp_power, *fp_energy;


	if (pid == 0) {
		printf("Freq\t Power\t Energy\t\n");
		while (1) {
			sprintf(freq_path, "/sys/rtes/freq", pid);
			sprintf(power_path, "/sys/rtes/power", pid);
			sprintf(energy_path, "/sys/rtes/energy", pid);


			fp_freq = fopen(freq_path, "r");
			if (fp_freq == NULL) {
				fprintf(stderr, "Not a valid pid (%d)\n", pid);
				exit(0);
			}
			sprintf(system_cmd, "/sbin/cat %s", freq_path);
			while(fread(freq, sizeof(char), DEFAULT_LEN, fp_freq)!=0);
			char* c = strstr(freq, "\n");
			*c = '\0'; // remove newline
			fclose(fp_freq);

			fp_power = fopen(power_path, "r");
			sprintf(system_cmd, "/sbin/cat %s", power_path);
			while(fread(power, sizeof(char), DEFAULT_LEN, fp_power)!=0);
			c = strstr(power, "\n");
			*c = '\0'; // remove newline
			fclose(fp_power);

			fp_energy = fopen(energy_path, "r");
			sprintf(system_cmd, "/sbin/cat %s", energy_path);
			while(fread(energy, sizeof(char), DEFAULT_LEN, fp_energy)!=0);
			c = strstr(energy, "\n");
			*c = '\0'; // remove newline
			fclose(fp_energy);

			printf("%s\t%s\t%s\t\n", freq, power, energy);
			usleep(1000000);
		}
	} else {
		printf("Energy\n");
		while(1) {
			sprintf(energy_path, "/sys/rtes/tasks/%d/energy", pid);
			fp_energy = fopen(energy_path, "r");
			if (fp_energy == NULL) {
				fprintf(stderr, "Not a valid pid (%d)\n", pid);
				exit(0);
			}
			sprintf(system_cmd, "/sbin/cat %s", energy_path);
			while(fread(energy, sizeof(char), DEFAULT_LEN, fp_energy)!=0);
			char* c = strstr(energy, "\n");
			*c = '\0'; // remove newline
			fclose(fp_energy);
			printf("%s\n", energy);
			usleep(1000000);
		}
	}




	
}
