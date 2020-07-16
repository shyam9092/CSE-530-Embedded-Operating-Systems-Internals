Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the EOSI Assignment-3 for modules.
Current directory has structures like below.
------------------------
EOSI-JOSHI-S-assgn02/
├── spi_device
│   ├── Makefile
│   └── spidevice.c
├── socket
│   ├── Makefile
│   ├── drv_genl.c
|   └── usr_genl.c
│   └── genl_lib.h
└── README.txt

2 directories, 6 files
--------------------------

=> Connect intel galileo board to your host machine using FTDI and ethernet cable

=> Open terminal for intel galileo board and enter into root of intel galileo board

=> Copy the above mentioned files created by Makefile to intel galileo board

=> Run the following command to install module into kernel

steps to run module
--------------------------
spi_device directory contains three files:
spidevice.c:     kernel code to install spidevice
Makefile:        Makefile to compile the spidevice

Follow steps to compile
=> Go to directory "spi_device" and run the make command like below

$ make

=> this will create a kernel loadable module and test binary like below,

$ ls
Makefile       Module.symvers  spi_device.ko     spi_device.mod.o
modules.order  spi_device.c    spi_device.mod.c  spi_device.o



$ insmod spi_device.ko


=> The above command does the following in kernel space:
=> It will create a spi device on bus number 1 called spidevice1.1




--------------------------
Socket directory contains the following files
drv_genl.c: Driver for spidev1.1 and for installing generic netlink
usr_genl.c: User program for testing generic netlink
genl_lib.h: Library for common data type usage
Makefile:  Makefile to compile the probe driver and test code

=> Go to Socket directory

=> Run the following command to install module into kernel 

$ insmod drv_genl.ko

$ ./output

=> The above command does the following in kernel space:
=> It will register a spi_driver for spi device 
=> It will establish a generic netlink connection in kernel
=> spi_device's probe function will be hit as spi_driver is loaded. It will configure gpios require to use spi functionality in kernel 
=> It will also create two threads: one to measure distance as required by user and the other to display the pattern
=> receive_message function will be called whenever user wants to send data 
=> User will send trigger,echo and chip select pin
=> It will send the display pattern data 
=> It will send a request to measure distance
=> The appropriate function is called to recieve the request 


<4>[   87.002383] entered init device 
<4>[   92.974495] entered probe function 
<4>[   95.132003] entered receive_message function 
<4>[   95.136606] message type is string 
<4>[   95.140269] User space message is CHS,000a 
<4>[   95.144494] Command is = CHS
<4>[   95.147571] Value is = 000a 
<4>[   95.150564] command is chip select 
<4>[   95.154089] 10 
<4>[   95.192985] Command/Data = 0x000f 
<4>[   95.210146] Command/Data = 0x010c 
<4>[   95.213886] Command/Data = 0x070b 
<4>[   95.240133] Command/Data = 0x0009 
<4>[   95.243873] Command/Data = 0x020a 
<4>[   95.253080] Command/Data = 0xff01 
<4>[   95.270144] Command/Data = 0xff02 
<4>[   95.282036] Command/Data = 0xff03 
<4>[   95.292022] Command/Data = 0xff04 
<4>[   95.302068] Command/Data = 0xff05 
<4>[   95.320145] Command/Data = 0xff06 
<4>[   95.332021] Command/Data = 0xff07 
<4>[   95.350144] Command/Data = 0xff08 
<4>[   95.362022] Command/Data = 0x0001 
<4>[   95.365964] Command/Data = 0x0002 
<4>[   95.390123] Command/Data = 0x0003 
<4>[   95.393862] Command/Data = 0x0004 
<4>[   95.403041] Command/Data = 0x0005 
<4>[   95.430116] Command/Data = 0x0006 
<4>[   95.433856] Command/Data = 0x0007 
<4>[   95.443095] Command/Data = 0x0008 
<4>[   95.470109] setup_dotmatrix is done 
<4>[   95.473732] return value of setup_dotmatrix is 0 
<4>[   95.495271] entered receive_message function 
<4>[   95.499887] message type is string 
<4>[   95.503550] User space message is TRG,0002 
<4>[   95.507774] Command is = TRG
<4>[   95.510944] Value is = 0002 
<4>[   95.513864] command is trigger 
<4>[   95.517027] 2 
<4>[   95.552061] entered receive_message function 
<4>[   95.556470] message type is string 
<4>[   95.559984] User space message is ECH,0001 
<4>[   95.564329] Command is = ECH
<4>[   95.567423] Value is = 0001 
<4>[   95.570418] command is echo 
<4>[   95.573334] 1 
<4>[   95.610755] entered receive_message function 
<4>[   95.615164] message type is string 
<4>[   95.618678] User space message is DLY,0064 
<4>[   95.623017] Command is = DLY 
<4>[   95.626109] Value is = 0064 
<4>[   95.656705] command is delay : 100 
<4>[   95.686151] entered receive_message function 
<4>[   95.690699] message type is custom structure 
<4>[   95.705149] entered receive_message function 
<4>[   95.709845] message type is custom structure 
<4>[   95.745876] entered receive_message function 
<4>[   95.750424] message type is custom structure 
<4>[   95.771573] entered receive_message function 
<4>[   95.776176] message type is string 
<4>[   95.779708] User space message is DST,0000 
<4>[   95.784050] Command is = DST� 
<4>[   95.787144] Value is = 0000 
<4>[   95.790140] ==> Command is Request Distance 
<4>[   95.812699] iteration = 0 
<4>[   95.900142] iteration = 1 
<4>[   95.992412] iteration = 2 
<4>[   96.080148] iteration = 3 
<4>[   96.170235] iteration = 4 
<4>[   96.260192] enetered calculate_average function 
<4>[   96.264862] distance = 61 
<4>[   96.267603] entered send_msg call 
<4>[   96.271157] header created 
<4>[   96.273992] nla_put done 
<4>[   96.276632] genlmsg_end done 
<4>[   96.279667] genlmsg_multicast done 
<4>[   99.291452] entered receive_message function 
<4>[   99.295861] message type is string 
<4>[   99.299375] User space message is DLY,0010 
<4>[   99.303719] Command is = DLY� 
<4>[   99.306898] Value is = 0010 
<4>[   99.330094] command is delay : 16 
<4>[   99.350943] entered receive_message function 
<4>[   99.355545] message type is string 
<4>[   99.359077] User space message is DST,0000 
<4>[   99.363446] Command is = DST� 
<4>[   99.366540] Value is = 0000 
<4>[   99.369447] ==> Command is Request Distance 
<4>[   99.392087] iteration = 0 
<4>[   99.482302] iteration = 1 
<4>[   99.572302] iteration = 2 
<4>[   99.662300] iteration = 3 
<4>[   99.752310] iteration = 4 
<4>[   99.842295] enetered calculate_average function 
<4>[   99.846964] distance = 4 
<4>[   99.849620] entered send_msg call 
<4>[   99.853178] header created 
<4>[   99.856011] nla_put done 
<4>[   99.858654] genlmsg_end done 
<4>[   99.861786] genlmsg_multicast done 
<5>[  100.140394] random: nonblocking pool is initialized
<4>[  102.871407] entered receive_message function 
<4>[  102.875814] message type is string 
<4>[  102.879328] User space message is DLY,00fa 
<4>[  102.883675] Command is = DLY
<4>[  102.886767] Value is = 00fa 
<4>[  102.910094] command is delay : 250 
<4>[  102.925031] entered receive_message function 
<4>[  102.929634] message type is string 
<4>[  102.933296] User space message is DST,0000 
<4>[  102.937521] Command is = DST� 
<4>[  102.940701] Value is = 0000 
<4>[  102.943623] ==> Command is Request Distance 
<4>[  102.961916] iteration = 0 
<4>[  103.050123] iteration = 1 
<4>[  103.140123] iteration = 2 
<4>[  103.230196] iteration = 3 
<4>[  103.320123] iteration = 4 

This is sample kernel log

configure nlsock done 
callback function set 
entered send_msg_to_kernel 
message type is string 
message sent 
entered send_msg_to_kernel 
message type is string 
message sent 
entered send_msg_to_kernel 
message type is string 
message sent 
entered send_msg_to_kernel 
message type is string 
message sent 
entered send_msg_to_kernel 
message type is string 
message sent 
entered print_kernel_msg function 
distance = 61 
 delay = 16 
entered send_msg_to_kernel 
message type is string 
message sent 
entered send_msg_to_kernel 
message type is string 
message sent 
entered print_kernel_msg function 
distance = 4 
 delay = 250 

This is sample user space log
