Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the EOSI Assignment for Part1 and Part2 modules.
Current directory has structures like below.
------------------------
EOSI-JOSHI-S-assgn01/
├── part1
│   ├── main.c
│   ├── Makefile
│   └── rbtraverse.c
├── part2
│   ├── main.c
│   ├── Makefile
│   └── rbprobe.c
└── README.txt

2 directories, 7 files
--------------------------


steps to run part1 modules
--------------------------
Part 1 modules contains three files:
rbtraverse.c: linux driver file
main.c:       test code to test the driver
Makefile:     Makefile to compile the driver code and test code

Follow steps to compile
=> Go to directory "part1" and run the make command like below

$ make

=> this will create a kernel loadable module and test binary like below,

$ ls
main.c  Makefile  modules.order  Module.symvers  rbtraverse.c  rbtraverse.ko  rbtraverse.mod.c  rbtraverse.mod.o  rbtraverse.o  rbtraverse_tester

=> Connect intel galileo board to your host machine using FTDI and ethernet cable

=> Open terminal for intel galileo board and enter into root of intel galileo board

=> Copy the above mentioned files created by Makefile to intel galileo board

=> Run the following command to install module into kernel

$ insmod rbtraverse.ko

=> Run the following command to test the module 

$ ./rbtraverse_tester

=> The above command creats four pthreads for two drivers respectively. 
=> Each driver will populate the rbtree by writing 50 nodes of key and data
=> Also, after 50 writes each pthread will run read,ioctl in random order

Thread [139673313138432] Write Success, Data [451] Key [238]
Thread [139673313138432] Write Success, Data [75] Key [130]
Thread [139673313138432] Write Success, Data [262] Key [125]
Thread [139673313138432] Write Success, Data [480] Key [253]
Thread [139673313138432] Write Success, Data [458] Key [128]
Thread [139673313138432] Write Success, Data [196] Key [204]
Thread [139673313138432] Write Success, Data [48] Key [65]
Thread [139673313138432] Write Success, Data [971] Key [95]
Thread [139673313138432] Write Success, Data [530] Key [245]
Thread [139673313138432] Write Success, Data [491] Key [107]
Thread [139673313138432] Write Success, Data [499] Key [172]
Thread [139673313138432] Write Success, Data [383] Key [229]
Thread [139673313138432] Write Success, Data [688] Key [54]
Thread [139673313138432] Read Success, Order [Ascending] Data [881] Key [38]
Thread [139673321531136] Read Success, Order [Descending] Data [235] Key [61]
Thread [139673338316544] Read Success, Order [Ascending] Data [37] Key [60]
Thread [139673329923840] Write Success, Data [77] Key [88]
Thread [139673313138432] Read Success, Order [Ascending] Data [711] Key [39]
Thread [139673321531136] Write Success, Data [338] Key [199]
Thread [139673338316544] Write Success, Data [818] Key [196]
Thread [139673329923840] Write Success, Data [946] Key [104]
Thread [139673313138432] Write Success, Data [346] Key [169]
Thread [139673321531136] Write Success, Data [649] Key [99]
Thread [139673338316544] Read Success, Order [Ascending] Data [150] Key [59]
Thread [139673329923840] Read Success, Order [Descending] Data [881] Key [38]
Thread [139673313138432] Write Success, Data [216] Key [225]
Thread [139673321531136] Write Success, Data [637] Key [81]
Thread [139673338316544] Read Success, Order [Ascending] Data [277] Key [58]
Thread [139673329923840] Read Success, Order [Descending] Data [363] Key [37]
Thread [139673313138432] Read Success, Order [Ascending] Data [778] Key [36]

This is a sample output log


steps to run part2 modules
--------------------------
rbprobe.c:      linux driver file for kprobe
rbtraverse.c:   linux driver file to populate rbtree 
main.c:         test code to test the probe driver
Makefile:       Makefile to compile the probe driver and test code

Follow steps to compile
=> Go to directory "part2" and run the make command like below

$ make

=> this will create a kernel loadable modules and test binary like below,

$ ls
main.c  Makefile  modules.order  Module.symvers  rbtraverse.c rbporbe.c  rbtraverse.ko  rbtraverse.mod.c  rbtraverse.mod.o  rbtraverse.o rbprobe.ko  rbprobe.mod.c  rbprobe.mod.o  rbprobe.o  rbtraverse_tester

=> Connect intel galileo board to your host machine using FTDI and ethernet cable

=> Open terminal for intel galileo board and enter into root of intel galileo board

=> Copy the above mentioned files created by Makefile to intel galileo board

=> Run the following command to install module into kernel

$ insmod rbtraverse.ko

$ insmode rbprobe.ko

=> Run the following command to test the module 

$ ./rbtraverse_tester





