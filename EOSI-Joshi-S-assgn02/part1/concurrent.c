#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <linux/ioctl.h>
#include <linux/rtc.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>

struct config_pins												//buffer to pass argument to ioctl
{
    int trigger_pin;
    int echo_pin;
};

struct set_param 													//buffer to pass argument to ioctl
{
    int m;
    int delta;
};

enum IOCTL_ARGS {														//ioctl command						
    CONFIG_PINS = 3,
    SET_PARAMETERS = 4
};

struct write_user {															//buffer to pass argument to write
    int write_arg;
};

struct read_user{														//buffer to read distance and time_stamp through read call
	unsigned long long distance;
	unsigned long long time_stamp;
};

int main(void)
{
	int fd,re;
	struct config_pins *configure;
	struct write_user *user_write;
	struct read_user *user_read;
	struct set_param *param;
	configure = (struct config_pins *)malloc(sizeof(struct config_pins));
	user_write = (struct write_user *)malloc(sizeof(struct write_user));
	user_read = (struct read_user *)malloc(sizeof(struct read_user));
	param = (struct set_param *)malloc(sizeof(struct set_param));
	configure->echo_pin = 3; 
	configure->trigger_pin = 4;
	user_write->write_arg = 0;
	param->m = 6;
	param->delta = 70;
	printf("echo pin = %d \t trigger pin = %d \n",configure->echo_pin,configure->trigger_pin);
	printf("m = %d \t delta = %d \n",param->m,param->delta);
 	
	fd = open("/dev/HCSR_2",O_RDWR);  				//Use HCSR_1 file to perform operations
	printf("fd = %d \n",fd);
	
	re = ioctl(fd,SET_PARAMETERS,param);			//send m and delta
	printf("return value from ioctl set_param = %d \n",re);
	
	re = ioctl(fd,CONFIG_PINS,configure);			//send trigger and echo pins
	printf("return value from ioctl config_pins = %d \n",re);

	sleep(5);
	re = write(fd,user_write,sizeof(struct write_user));   //trigger 
	printf("return value of write = %d \n",re);

	re = read(fd,user_read,sizeof(struct read_user));
	printf("distance = %llu \t time_stamp = %llu \n",user_read->distance,user_read->time_stamp);

	return 0;
}