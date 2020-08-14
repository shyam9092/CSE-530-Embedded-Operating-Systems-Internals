Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the EOSI Assignment-2 for Part1 and Part2 modules.
Current directory has structures like below.
------------------------
EOSI-JOSHI-S-assgn02/
├── part1
│   ├── main.c
│   ├── Makefile
│   └── hc_sr04driver.c
│   └── concurrent.c
│   └── concurrent
├── part2
│   ├── Makefile
│   ├── Sample_platform_device.h
|   └── platform_device.c 
│   └── platform_driver.c
└── README.txt

2 directories, 10 files
--------------------------


steps to run part1 module
--------------------------
Part 1 modules contains three files:
hc_sr04driver.c: linux driver file
main.c:          test code to test the driver
Makefile:        Makefile to compile the driver code and test code
concurrent.c:    test code to test the concurrency of the module (to be used with main.c)
concurrent:      executable file of concurrent.c

Follow steps to compile
=> Go to directory "part1" and run the make command like below

$ make

=> this will create a kernel loadable module and test binary like below,

$ ls
hc_sr04driver.c      hc_sr04driver.mod.o  Makefile        output
hc_sr04driver.ko     hc_sr04driver.o      modules.order
hc_sr04driver.mod.c  main.c               Module.symvers


=> Connect intel galileo board to your host machine using FTDI and ethernet cable

=> Open terminal for intel galileo board and enter into root of intel galileo board

=> Copy the above mentioned files created by Makefile to intel galileo board

=> Run the following command to install module into kernel

$ insmod hc_sr04driver.ko  num=5

=> Run the following command to test the module 

$ ./output

=> The above command does the following in kernel space:
=> It will create 5 instance of per-device structrues and installs 5 miscellaneous device drivers

=> The above command does the following in user space:
=> It will open the file descriptor of HCSR_1 to be used in ioctl,read,write functions.
=> It will send echo pin-2, trigger pin-0, m-5, delta-60 to module through ioctl.
=> It will set 0 as write argument (for trigger to happen) and then call the write function which will calculate the distance and then store it in fifo_buffer.
=> It will set 0 as write argument to clear the buffer and then call write function.
=> Then read function is called to check if it triggers measurement on its own.
=> It will call write function consecutively 5 times and then read function to get the distance and time stamp and also to check working of fifo_buffer.
=> It will then change the value of echo pin, trigger pin, m, delta and call ioctl function for kernel to configure them.
=> Then write and read call are performed to check working of above configurations.



<4>[   55.570425] entered init function 
<4>[   55.573880] misc-device.name  = HCSR_1
<4>[   55.590470] successfully registered and minor number for hscr_[1] is :57
<4>[   55.610127] exiting init 
<4>[   58.800459] entered open 
<4>[   58.803323] minor number found: 57 
<4>[   58.806849] --------------------BYE BYE-------------------------- 
<4>[   58.813200] minor number found for user space open call = 57 
<4>[   58.841873] entered ioctl function 
<4>[   58.845600] entered SET_PARAMETER CALL 
<4>[   58.849481] m = 5 	 delta = 60 
<4>[   58.870135] exiting ioctl 
<4>[   58.883058] entered ioctl function 
<4>[   58.886645] trigger_pin = 0 	 echo_pin = 2 
<4>[   58.890989] =========INSIDE CONFIG PIN FUNCTION========= 
<4>[   58.896431] 0 
<4>[   58.912070] 2 
<4>[   58.931201] exiting configure_gpio_pins 
<4>[   58.935366] counter_trig_globabl = 2 	 counter_ech_global = 3 
<4>[   58.941372] =========EXIT CONFIG PIN FUNCTION========= 
<4>[   58.946643] return value of configure_gpio_pins = 1 
<4>[   58.951727] exiting ioctl 
<4>[   63.972084] write value = 0 
<4>[   63.975007] trigger on going flag is set 
<4>[   63.979855] iteration = 1 
<4>[   65.060141] iteration = 2 
<4>[   66.140143] iteration = 3 
<4>[   67.220143] iteration = 4 
<4>[   68.300142] iteration = 5 
<4>[   69.380136] iteration = 6 
<4>[   70.460144] iteration = 7 
<4>[   71.540139] final stage 
<4>[   71.542719] distance = 23 
<4>[   71.545450] time_stamp = 37677157108 
<4>[   71.549134] entered write_fifo 
<4>[   71.552419] done and dusted 
<4>[   73.994111] write value = 1 
<4>[   73.997032] clear buffer data 
<4>[   74.012021] iteration = 1 
<4>[   75.090138] iteration = 2 
<4>[   76.170180] iteration = 3 
<4>[   77.250144] iteration = 4 
<4>[   78.330144] iteration = 5 
<4>[   79.410141] iteration = 6 
<4>[   80.490143] iteration = 7 
<4>[   81.570139] final stage 
<4>[   81.572720] distance = 23 
<4>[   81.575450] time_stamp = 41679889606 
<4>[   81.579134] entered write_fifo 
<4>[   81.582430] done and dusted 
<4>[   91.590433] write value = 0 
<4>[   91.593355] trigger on going flag is set 
<4>[   91.598204] iteration = 1 
<4>[   92.680147] iteration = 2 
<4>[   93.760143] iteration = 3 
<4>[   94.840143] iteration = 4 
<4>[   95.920125] iteration = 5 
<4>[   97.000136] iteration = 6 
<4>[   98.080136] iteration = 7 
<4>[   99.160197] final stage 
<4>[   99.162777] distance = 23 
<4>[   99.165509] time_stamp = 48699659398 
<4>[   99.169194] entered write_fifo 
<4>[   99.172483] done and dusted 
<4>[  101.608843] write value = 0 
<4>[  101.622019] trigger on going flag is set 
<4>[  101.628103] iteration = 1 
<4>[  102.710143] iteration = 2 
<4>[  103.790143] iteration = 3 
<4>[  104.870151] iteration = 4 
<4>[  105.950146] iteration = 5 
<4>[  107.030126] iteration = 6 
<4>[  108.110138] iteration = 7 
<4>[  109.190139] final stage 
<4>[  109.192720] distance = 23 
<4>[  109.195450] time_stamp = 52702368634 
<4>[  109.199134] entered write_fifo 
<4>[  109.202420] done and dusted 
<4>[  111.628177] write value = 0 
<4>[  111.643355] trigger on going flag is set 
<4>[  111.649422] iteration = 1 
<5>[  112.660743] random: nonblocking pool is initialized
<4>[  112.730127] iteration = 2 
<4>[  113.810144] iteration = 3 
<4>[  114.890125] iteration = 4 
<4>[  115.970146] iteration = 5 
<4>[  117.050126] iteration = 6 
<4>[  118.130143] iteration = 7 
<4>[  119.210139] final stage 
<4>[  119.212720] distance = 19 
<4>[  119.215451] time_stamp = 56701110254 
<4>[  119.219135] entered write_fifo 
<4>[  119.222421] done and dusted 
<4>[  121.649495] write value = 0 
<4>[  121.662016] trigger on going flag is set 
<4>[  121.667788] iteration = 1 
<4>[  122.750143] iteration = 2 
<4>[  123.830144] iteration = 3 
<4>[  124.910126] iteration = 4 
<4>[  125.990218] iteration = 5 
<4>[  127.070132] iteration = 6 
<4>[  128.150144] iteration = 7 
<4>[  129.230139] final stage 
<4>[  129.232720] distance = 23 
<4>[  129.235452] time_stamp = 60699851782 
<4>[  129.239136] entered write_fifo 
<4>[  129.242421] done and dusted 
<4>[  131.667873] write value = 0 
<4>[  131.682048] trigger on going flag is set 
<4>[  131.697931] iteration = 1 
<4>[  132.780144] iteration = 2 
<4>[  133.860143] iteration = 3 
<4>[  134.940144] iteration = 4 
<4>[  136.020125] iteration = 5 
<4>[  137.100137] iteration = 6 
<4>[  138.180143] iteration = 7 
<4>[  139.260139] final stage 
<4>[  139.262719] distance = 23 
<4>[  139.265450] time_stamp = 64702583656 
<4>[  139.269135] entered write_fifo 
<4>[  139.272432] done and dusted 
<4>[  139.280293] entered ioctl function 
<4>[  139.283832] trigger_pin = 6 	 echo_pin = 9 
<4>[  139.288041] =========INSIDE CONFIG PIN FUNCTION========= 
<4>[  139.293569] 6 
<4>[  139.331485] 9 
<4>[  139.342964] exiting configure_gpio_pins 
<4>[  139.346935] counter_trig_globabl = 3 	 counter_ech_global = 3 
<4>[  139.352927] =========EXIT CONFIG PIN FUNCTION========= 
<4>[  139.358200] return value of configure_gpio_pins = 1 
<4>[  139.363280] exiting ioctl 
<4>[  139.383330] entered ioctl function 
<4>[  139.387050] entered SET_PARAMETER CALL 
<4>[  139.391058] m = 3 	 delta = 65 
<4>[  139.400155] exiting ioctl 
<4>[  139.420525] write value = 0 
<4>[  139.423656] trigger on going flag is set 
<4>[  139.429854] iteration = 1 
<4>[  140.520134] iteration = 2 
<4>[  141.610134] iteration = 3 
<4>[  142.700135] iteration = 4 
<4>[  143.790135] iteration = 5 
<4>[  144.880118] final stage 
<4>[  144.882697] distance = 25 
<4>[  144.885428] time_stamp = 66945382214 
<4>[  144.889112] entered write_fifo 
<4>[  144.892409] done and dusted 


This is a sample kernel log

echo pin = 2 	 trigger pin = 0 
m = 5 	 delta = 60 
fd = 3 
return value from ioctl set_param = 0 
return value from ioctl config_pins = 0 
return value of write = 0 
return value of write = 0 
distance = 23 	 time_stamp = 41679889606 
return value of write = 0 
return value of write = 0 
return value of write = 0 
return value of write = 0 
return value of write = 0 
distance = 23 	 time_stamp = 48699659398 
distance = 23 	 time_stamp = 52702368634 
distance = 19 	 time_stamp = 56701110254 
distance = 23 	 time_stamp = 60699851782 
distance = 23 	 time_stamp = 64702583656 
return value from ioctl set_param = 0 
return value from ioctl set_param = 0 
return value of write = 0 
distance = 25 	 time_stamp = 66945382214 

This is a sample user log





steps to run part2 modules
--------------------------
platform_device.c:          linux driver file for platform device 
platform_driver.c:          linux driver file for platform driver
Sample_platform_device.h :  library to share commonly used structure 
Makefile:  Makefile to compile the probe driver and test code


=> Run the following command to install module into kernel

$ insmod platform_driver.ko num=1
$ insmod platform_device.ko

=> The above command does the following in kernel space:
=> It will register a platform_device, a miscellaneous device and creat per-device structures
=> It will create nodes in /sys/devices/platform
=> platform_driver's probe function will be hit as platform_device is installed. 
=> It will create class called HCSR and add HCSR_1 device in /sys/class directory
=> It will create distance,enable,echo,number_samples,sampling_period,trigger nodes in HCSR_1 .

root@quark:/sys/devices/platform# ls
GalileoGen2             intel-qrk-esram.0       power
HCSR_1                  intel-qrk-imr           qrk-gpio-restrict-nc.0
PNP0C0C:00              intel-qrk-thrm.0        qrk-gpio-restrict-sc.0
PNP0C0E:00              intel_qrk_imr_test      reg-dummy
alarmtimer              pcspkr                  regulatory.0
intel-qrk-ecc.0         platform-framebuffer.0  uevent

root@quark:/sys/class# ls
HCSR          dma           i2c-dev       misc          ppp           rtc           udc
backlight     dmi           ieee80211     mmc_host      pps           scsi_device   uio
bdi           gpio          input         net           ptp           scsi_host     vc
block         graphics      iommu         pci_bus       pwm           spi_master    vtconsole
bluetooth     hidraw        leds          phy           qrk_imr_test  spidev        watchdog
bsg           hwmon         mdio_bus      power_supply  regulator     thermal
devcoredump   i2c-adapter   mem           powercap      rfkill        tty

root@quark:/sys/class/HCSR# ls
HCSR_1

root@quark:/sys/class/HCSR/HCSR_1# ls
distance         enable           power            subsystem        uevent
echo             number_samples   sampling_period  trigger

$ chmod +x syfs.sh
$ ./syfs.sh