IOT_HOME = /opt/iot-devkit/1.7.2/sysroots
PWD:= $(shell pwd)
FLAGS=-WALL
KDIR:=$(IOT_HOME)/i586-poky-linux/usr/src/kernel

export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)

CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=$(IOT_HOME)/i586-poky-linux/

obj-m = sudo_driver.o
APP = output

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $(FLAGS) -C $(KDIR) M=$(PWD) modules
	$(CC) -o $(APP) test.c  
		
clean:
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(SROOT)/usr/src/kernel M=$(PWD) clean
