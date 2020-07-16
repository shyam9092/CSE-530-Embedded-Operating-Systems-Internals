Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the EOSI Assignment-4 for modules and syscall.
Current directory has structures like below.
------------------------
EOSI-JOSHI-S-assgn04/
├── patch_file
│   ├── Shyam_assgn4.patch
├── sudo_dumpstack.c 
├── test.c
├── Makefile
└── README.txt

1 directories, 5 files
--------------------------

=> Connect intel galileo board to your host machine using FTDI and ethernet cable

=> Open terminal for intel galileo board and enter into root of intel galileo board

=> Copy the above mentioned files created by Makefile to intel galileo board

=> Run the following command to install module into kernel

steps to test system call
--------------------------
sudo_driver.c:     dummy driver for user program to put kprobes 
test.c:            user program to call insdump 
Makefile:          Makefile to compile the sudo_driver.c and test.c

Follow steps to compile
=> Go to directory "EOSI-JOSHI-S-assgn04" and run the make command like below

$ make

=> this will create a kernel loadable module and test binary like below,

$ ls
Makefile       Module.symvers  sudo_driver.ko     sudo_driver.mod.o
modules.order  sudo_driver.c    sudo_driver.mod.c  sudo_driver.o output



$ insmod sudo_driver.ko


=> The above command does the following things in kernel:
=> It will create a character driver in the kernel
=> It will register the character driver with four file operations (read,write,open,release)

$ ./output

=>It will put kprobes at open,read,write of the character driver by calling insdump and then it will call those function (open,read,write)



Follow the following steps in patch_file directory

$patch -p1 < ../EOSI-JOSHI-S-assgn04/patch_file/Shyam_assgn4.patch

$PATH=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$PATH

$ARCH=x86 LOCALVERSION= CROSS_COMPILE=i586-poky-linux- make -j4

Go to kernel/arch/x86/boot and copy bzImage to /media/realroot of sdcard

$reboot