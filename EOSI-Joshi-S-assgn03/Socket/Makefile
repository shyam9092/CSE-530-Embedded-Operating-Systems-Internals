CC = i586-poky-linux-gcc
FLAGS=-Wall
LFLAGS=-lpthread
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SDKTARGETSYSROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)
KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel

LDLIBS = -L$(SDKTARGETSYSROOT)/usr/lib
CCFLAGS = -I$(SDKTARGETSYSROOT)/usr/include/libnl3

APP = output

obj-m:= drv_genl.o

all:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(shell pwd) modules
	$(CC) -o $(APP) usr_genl.c $(LDLIBS) $(CCFLAGS) --sysroot=$(SDKTARGETSYSROOT) -lnl-genl-3 -lnl-3


clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP) 
