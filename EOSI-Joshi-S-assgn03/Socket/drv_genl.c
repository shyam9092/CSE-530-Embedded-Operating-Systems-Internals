#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/timer.h>
#include <linux/export.h>
#include <net/genetlink.h>
#include <linux/module.h>
#include <net/sock.h> 
#include <linux/netlink.h>
#include <linux/skbuff.h> 
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/msr.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <asm/div64.h>
#include <asm/delay.h>
#include <linux/stat.h>
#include <linux/threads.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/math64.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include "genl_lib.h"


struct led_data{                                                                // To display animation of ball with 3 patterns
    u8 led1[8]; // 8 bytes (64 bits) for 64 leds
    u8 led2[8]; // 8 bytes (64 bits) for 64 leds
    u8 led3[8]; // 8 bytes (64 bits) for 64 leds
}*pattern;


struct per_device{
    int echo_pin, chip_select;                                                  //Global structure for various function usage
    unsigned long long time_rise[5];                                            //time stamp for rising edge    
    unsigned long long time_fall[5];                                            //time stamp for falling edge
    unsigned long long distance;                                                //final distance value
    int counter_irq;                                                             // counter_irq for irq_handler
    struct task_struct *worker_thread;                                          //worker thread for distance measurement
    struct task_struct *display_thread;                                          //display thread to continously display pattern
}*global_struct;

struct spi_transfer t;                                                             //spi tail message
struct sock *sk = NULL;                                                         //sock structure to initialize socket
static struct spi_device *spidev_buff;                                          //for usage in spi_probe function
static uint32_t speed = 500000;                                                 //clock for spi
struct spi_message msg;                                                         //spi message to be initialized
static unsigned char tx_buff[2];                                                //buffer to be linked to spi_transfer
struct mutex display_mutex;                                                    //lock to be used for display related work
static int display_ready;                                                       //flag for display
static int mSecDelay;                                                           //delay between animations
struct mutex write_mutex;                                                       //lock to trigger distance measurement inside worker thread 
static int measurement_ongoing;                                                  //flag for worker thread 
static int trigger_pin;                                                             //trigger pin as given by user

static const struct spi_device_id device_table[] = {                               //spi_device_id for linking the spidevice
    { "MAX7219", 0 },
    { }
};

static int irq_num;                     //interrupt number to be requested and later used to request irq 
static int flag_irq = 0;               //used in irq_handler function for rising and falling edge interrupt

static int gpio_1;                      //local variables to store all the gpio_request pins
static int gpio_2;
static int gpio_3;
static int gpio_4;
static int gpio_5;
static int gpio_6;
static int gpio_7;
static int gpio_8;
static int gpio_9;
static int gpio_10;
static int gpio_11;
static int gpio_12;

static int counter_cs;                //to be used to initialize chip select and dotmatrix led
static int counter_echo;                // counters to be used in exit function for gpio_free calls
static int counter_trigger;

static int cs_flag = 0;                 //chip select flag to be used for the first time during user chip select call
static int dotmatrix_pattern = 0;        //for displaying three different patterns in two sequence to dotmatrix


static struct genl_family family_genl;      //genl_family member to be registered


static int patternIter = 0;             // for displaying 3 patterns to dotmatrix in 2 sequence

static void send_msg(int distance)          //used to send measured distance to user
{
    void *header;
    int ret;
    int flags = GFP_ATOMIC;
    struct sk_buff *skb = genlmsg_new(NLMSG_DEFAULT_SIZE,flags);
    printk("entered send_msg call \n");

    header = genlmsg_put(skb,0,0,&family_genl,flags,GENL_TEST_C_MSG);
    printk("header created \n");
    ret = nla_put(skb,GENL_INT_ATTR,sizeof(int),&distance);
    printk("nla_put done \n");
    genlmsg_end(skb,header);
    printk("genlmsg_end done \n");
    genlmsg_multicast(&family_genl,skb,0,0,flags);
    printk("genlmsg_multicast done \n");
}

void calculate_average(void)            //calculate average of 5 sample readings
{   
    int iter=0;
    unsigned long long time_elapsed[30];
    unsigned long long time_elapsed_average = 0;
    unsigned long long div = 11764;
    printk("enetered calculate_average function \n");
    for(iter=0;iter<5;iter++)
    {
        time_elapsed[iter] = ((global_struct->time_fall[iter]) - (global_struct->time_rise[iter]));       //get the time difference between rising and falling for respective readings
        time_elapsed[iter] = div_u64(time_elapsed[iter],2);
        time_elapsed_average = time_elapsed_average + time_elapsed[iter];
    }
    global_struct->counter_irq = 0;
    for(iter=0;iter<5;iter++)
    {
        global_struct->time_fall[iter]=0;
        global_struct->time_rise[iter]=0;
    }
    time_elapsed_average = div_u64(time_elapsed_average,5);
    time_elapsed_average = div_u64(time_elapsed_average,div);
    printk("distance = %llu \n",time_elapsed_average);
    global_struct->distance = time_elapsed_average;
    send_msg((int)(global_struct->distance));
    
}

int setup_dotmatrix(int isDynamicPattern)           //to send initial commands for configuring dotmatrix and further to display patterns
{
    int iter,ret;
    int iterSize = 20;
    u8 cmd_addr[21] = {0x0F,0x0C,0x0B,0x09,0x0A,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};  //address of the commands and data
    u8 cmd_value[21] = {0x00,0x01,0x07,0x00,0x02,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};   //corresponding values of the command
    t.len = 2;              //tail of spi message initialization
    t.tx_buf = &tx_buff;
    t.rx_buf = 0;
    t.cs_change = 1;
	t.bits_per_word = 8; 
    t.speed_hz = speed;
    for(iter=0;iter<=iterSize;iter++)
    {
        tx_buff[0] = cmd_addr[iter];
        tx_buff[1] = cmd_value[iter];
        printk("Command/Data = 0x%02x%02x \n", tx_buff[1], tx_buff[0]);
        spi_message_init(&msg);
        spi_message_add_tail(&t, &msg);
        gpio_set_value_cansleep(global_struct->chip_select,0);
        ret = spi_sync(spidev_buff, &msg);
        gpio_set_value_cansleep(global_struct->chip_select,1);
    }
    printk("setup_dotmatrix is done \n");
    return 0;
}


static int display_pattern(void *args)
{
    int display;
    int iterSize = 7;
    int iter,ret;
    u8 cmd_addr[8] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};         //address to be used when displaying different patterns
    // u8 local_led1[8] = {0x00,0x00,0x1C,0x3E,0x3E,0x3E,0x3E,0x1C};
    // u8 local_led2[8] = {0x1C,0x3E,0x3E,0x3E,0x3E,0x1C,0x00,0x00};
    // u8 local_led3[8] =  {0x00,0x1C,0x3E,0x3E,0x3E,0x3E,0x1C,0x00};
    while(!kthread_should_stop()) 
    {
        mutex_lock(&(display_mutex));
        display = display_ready;                //check if the display pattern is available
        mutex_unlock(&(display_mutex));
        if(display)                                                          //check whether trigger_on going flag is set by write function, if yes then trigger m+2 times the trigger pin
        {
            t.len = 2;              //tail of spi message initialization
            t.tx_buf = &tx_buff;
            t.rx_buf = 0;
            t.cs_change = 1;
            t.bits_per_word = 8; 
            t.speed_hz = speed;
            for(iter=0;iter<=iterSize;iter++)
            {
                if(((dotmatrix_pattern%5) == 0) || ((dotmatrix_pattern%5) == 4)) {
                    tx_buff[1] = pattern->led1[iter];
                } else if(((dotmatrix_pattern%5) == 1) || ((dotmatrix_pattern%5) == 3)) {
                    tx_buff[1] = pattern->led2[iter];
                } else if(((dotmatrix_pattern%5) == 2)) {
                    tx_buff[1] = pattern->led3[iter];
                }
                tx_buff[0] = cmd_addr[iter];
                spi_message_init(&msg);
                spi_message_add_tail(&t, &msg);
                gpio_set_value_cansleep(global_struct->chip_select,0);
                ret = spi_sync(spidev_buff, &msg);
                gpio_set_value_cansleep(global_struct->chip_select,1);
            }
            msleep(mSecDelay);          //give delays between simultaneous patterns
            dotmatrix_pattern++;
        }
        
    }
    do_exit(0);
    return 0;
}

static int get_measurement(void *args)                                               //worker_thread's function
{ 
    while(!kthread_should_stop())
    {
        int i;
        int isRunning;
        mutex_lock(&(write_mutex));
        isRunning = measurement_ongoing;
        mutex_unlock(&(write_mutex));
        if(isRunning)                                            //check whether trigger_on going flag is set by write function, if yes then trigger m+2 times the trigger pin
        {
            for(i=0;i<5;i++)                                          //trigger m+2 times
            {  
                printk("iteration = %d \n",i);
                gpio_set_value(trigger_pin,1);             // set 1 to trigger pin       
                msleep(5);
                gpio_set_value(trigger_pin,0); 
                msleep(60);
            }
            mutex_lock(&(write_mutex));
            measurement_ongoing = 0; // set flag to 0 to indicate that trigger call is completed
            mutex_unlock(&(write_mutex));
            calculate_average();
        }
    }
    do_exit(0);
    return 0;
}

static irq_handler_t irq_handler(int irq_n,struct per_device *ptr)               //interrupt handler function
{
    struct per_device *buff;
    buff = (struct per_device *)ptr; 
    
    if(flag_irq == 0)
    {   
        buff->time_rise[buff->counter_irq] = native_read_tsc();     //record time for rising edge
        irq_set_irq_type(irq_n,IRQ_TYPE_EDGE_FALLING);              //change interrupt type to falling edge 
        flag_irq = 1;
    }
    else
    {   
        buff->time_fall[buff->counter_irq] = native_read_tsc();     //record time for falling edge
        irq_set_irq_type(irq_n,IRQ_TYPE_EDGE_RISING);               //change interrupt type to rising edge
        flag_irq = 0; 
        buff->counter_irq++;
    }
   return (irq_handler_t) IRQ_HANDLED;
}



int spi_gpio_configure(void)                    //configuring spi_pins when spi_device's probe function is called 
{
    int ret;
    int gpio_spi_1,gpio_spi_2,gpio_spi_3;
    
    gpio_spi_1 = 24;
    gpio_spi_2 = 44;
    gpio_spi_3 = 72;
    ret = gpio_request(gpio_spi_1,"gpio24_dir_out");
    if(ret)
    {
        printk("Bad GPIO Request %d \n",gpio_spi_1);
        return 0;
    }
    ret = gpio_request(gpio_spi_2,"gpio44_mux");
    if(ret)
    {
        printk("Bad GPIO Request %d \n",gpio_spi_2);
        return 0;
    }
    ret = gpio_request(gpio_spi_3,"gpio72_mux");
    if(ret)
    {
        printk("Bad GPIO Request %d \n",gpio_spi_3);
        return 0;
    }
    gpio_set_value_cansleep(gpio_spi_3,0);
    gpio_direction_output(gpio_spi_2,0);
    gpio_set_value_cansleep(gpio_spi_2,1);
    gpio_direction_output(gpio_spi_1,0);

    
    gpio_spi_1 = 42;
    ret = gpio_request(gpio_spi_1,"gpio42_dir_out");
    if(ret)
    {
        printk("Bad GPIO Request %d \n",gpio_spi_1);
        return 0;
    }
    gpio_direction_output(gpio_spi_1,0);

    gpio_spi_1 = 30;
    gpio_spi_2 = 46;
    ret = gpio_request(gpio_spi_1,"gpio30_dir_out");
    if(ret)
    {
        printk("Bad GPIO Request %d \n",gpio_spi_1);
        return 0;
    }
    ret = gpio_request(gpio_spi_2,"gpio46_mux");
    if(ret)
    {
        printk("Bad GPIO Request %d \n",gpio_spi_2);
        return 0;
    }
    gpio_direction_output(gpio_spi_2,1);
    gpio_set_value_cansleep(gpio_spi_2,1);
    gpio_direction_output(gpio_spi_1,0);
    
    ret = 0;
    return ret;
}

int chipselect_configure(int cs)                //configuring chip_select pin
{   
    int re;
    switch(cs)
    {
        case 0:
        gpio_9 = 11;
        gpio_10 = 32;
      
        printk("0 \n");
        re = gpio_request(gpio_9,"gpio11_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio32_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 2;
        break;

        case 1:
        gpio_9 = 12;
        gpio_10 = 28;
        gpio_11 = 45;
      
        printk("1 \n");
        re = gpio_request(gpio_9,"gpio12_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio28_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio45_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_direction_output(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 2:
        gpio_9 = 13;
        gpio_10 = 34;
        gpio_11 = 77;
      
        printk("2 \n");
        re = gpio_request(gpio_9,"gpio13_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio34_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio77_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 3:
        gpio_9 = 14;
        gpio_10 = 16;
        gpio_11 = 76;
        gpio_12 = 64;
      
        printk("3 \n");
        re = gpio_request(gpio_9,"gpio14_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio16_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio76_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        re = gpio_request(gpio_12,"gpio64_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_12);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_12,0);
        gpio_set_value_cansleep(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 4;
        break;

        case 4:
        gpio_9 = 6;
        gpio_10 = 36;
      
        printk("4 \n");
        re = gpio_request(gpio_9,"gpio6_out");
        if(re)
        { 
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio36_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 2;
        break;

        case 5:
        gpio_9 = 0;
        gpio_10 = 18;
        gpio_11 = 66;
      
        printk("5 \n");
        re = gpio_request(gpio_9,"gpio0_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio16_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio76_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_direction_output(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 6:
        gpio_9 = 1;
        gpio_10 = 20;
        gpio_11 = 68;
      
        printk("6 \n");
        re = gpio_request(gpio_9,"gpio1_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio20_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio68_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 7:
        gpio_9 = 38;
      
        printk("7 \n");
        re = gpio_request(gpio_9,"gpio38_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        gpio_direction_output(gpio_9,0);
        counter_cs = 1;
        break;

        case 8:
        gpio_9 = 40;
      
        printk("8 \n");
         re = gpio_request(gpio_9,"gpio40_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        gpio_direction_output(gpio_9,0);
        counter_cs = 1;
        break;

        case 9:
        gpio_9 = 4;
        gpio_10 = 22;
        gpio_11 = 70;
      
        printk("9 \n");
        re = gpio_request(4,"gpio4_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(22,"gpio22_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(70,"gpio70_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 10:
        gpio_9 = 10;
        gpio_10 = 26;
        gpio_11 = 74;
      
        printk("10 \n");
        re = gpio_request(gpio_9,"gpio10_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio26_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio74_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_11,0);
        gpio_direction_output(gpio_10,0);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 14:
        gpio_9 = 48;
      
        printk("14 \n");
        re = gpio_request(gpio_9,"gpio48_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        gpio_direction_output(gpio_9,0);
        counter_cs = 1;
        break;

        case 15:
        gpio_9 = 50;
      
        printk("15 \n");
        re = gpio_request(gpio_9,"gpio50_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        gpio_direction_output(gpio_9,0);
        counter_cs = 1;
        break;

        case 16:
        gpio_9 = 52;
      
        printk("16 \n");
        re = gpio_request(gpio_9,"gpio52_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        gpio_direction_output(gpio_9,0);
        counter_cs = 1;
        break;

        case 17:
        gpio_9 = 54;
      
        printk("17 \n");
        re = gpio_request(gpio_9,"gpio54_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        gpio_direction_output(gpio_9,0);
        counter_cs = 1;
        break;

        case 18:
        gpio_9 = 56;
        gpio_10 = 60;
        gpio_11 = 78;
      
        printk("18 \n");
        re = gpio_request(gpio_9,"gpio56_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio60_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio78_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_direction_output(gpio_11,0);
        gpio_set_value(gpio_11,1);
        gpio_direction_output(gpio_10,0);
        gpio_set_value(gpio_10,1);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;

        case 19:
        gpio_9 = 58;
        gpio_10 = 60;
        gpio_11 = 79;
      
        printk("19 \n");
        re = gpio_request(gpio_9,"gpio56_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_9);
            return 0;
            break;
        }
        re = gpio_request(gpio_10,"gpio60_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_10);
            return 0;
            break;
        }
        re = gpio_request(gpio_11,"gpio78_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_11);
            return 0;
            break;
        }
        gpio_direction_output(gpio_11,0);
        gpio_set_value(gpio_11,1);
        gpio_direction_output(gpio_10,0);
        gpio_set_value(gpio_10,1);
        gpio_direction_output(gpio_9,0);
        counter_cs = 3;
        break;
    }
    global_struct->chip_select = gpio_9;
    return 0;
}

int trigger_pin_configure(int trig)         //configuring triggger pin
{
    int re;
    switch (trig)
    {
        case 0:
        gpio_1 = 11;
        gpio_2 = 32;
      
        printk("0 \n");
        re = gpio_request(gpio_1,"gpio11_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio32_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 2;
        break;

        case 1:
        gpio_1 = 12;
        gpio_2 = 28;
        gpio_3 = 45;
      
        printk("1 \n");
        re = gpio_request(gpio_1,"gpio12_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio28_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio45_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_direction_output(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 2:
        gpio_1 = 13;
        gpio_2 = 34;
        gpio_3 = 77;
      
        printk("2 \n");
        re = gpio_request(gpio_1,"gpio13_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio34_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio77_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 3:
        gpio_1 = 14;
        gpio_2 = 16;
        gpio_3 = 76;
        gpio_4 = 64;
      
        printk("3 \n");
        re = gpio_request(gpio_1,"gpio14_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio16_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio76_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        re = gpio_request(gpio_4,"gpio64_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_4);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_4,0);
        gpio_set_value_cansleep(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 4; 
        break;

        case 4:
        gpio_1 = 6;
        gpio_2 = 36;
      
        printk("4 \n");
        re = gpio_request(gpio_1,"gpio6_out");
        if(re)
        { 
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio36_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 2;
        break;

        case 5:
        gpio_1 = 0;
        gpio_2 = 18;
        gpio_3 = 66;
      
        printk("5 \n");
        re = gpio_request(gpio_1,"gpio0_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio16_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio76_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_direction_output(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 6:
        gpio_1 = 1;
        gpio_2 = 20;
        gpio_3 = 68;
      
        printk("6 \n");
        re = gpio_request(gpio_1,"gpio1_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }counter_trigger = 3;
        re = gpio_request(gpio_2,"gpio20_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio68_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 7:
        gpio_1 = 38;
      
        printk("7 \n");
        re = gpio_request(gpio_1,"gpio38_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        gpio_direction_output(gpio_1,0);
        counter_trigger = 1;
        break;

        case 8:
        gpio_1 = 40;
      
        printk("8 \n");
         re = gpio_request(gpio_1,"gpio40_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        gpio_direction_output(gpio_1,0);
        counter_trigger = 1;
        break;

        case 9:
        gpio_1 = 4;
        gpio_2 = 22;
        gpio_3 = 70;
      
        printk("9 \n");
        re = gpio_request(4,"gpio4_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(22,"gpio22_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(70,"gpio70_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 10:
        gpio_1 = 10;
        gpio_2 = 26;
        gpio_3 = 74;
      
        printk("10 \n");
        re = gpio_request(gpio_1,"gpio10_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio26_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio74_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 14:
        gpio_1 = 48;
      
        printk("14 \n");
        re = gpio_request(gpio_1,"gpio48_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        gpio_direction_output(gpio_1,0);
        counter_trigger = 1;
        break;

        case 15:
        gpio_1 = 50;
      
        printk("15 \n");
        re = gpio_request(gpio_1,"gpio50_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        gpio_direction_output(gpio_1,0);
        counter_trigger = 1;
        break;

        case 16:
        gpio_1 = 52;
      
        printk("16 \n");
        re = gpio_request(gpio_1,"gpio52_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        gpio_direction_output(gpio_1,0);
        counter_trigger = 1;
        break;

        case 17:
        gpio_1 = 54;
      
        printk("17 \n");
        re = gpio_request(gpio_1,"gpio54_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        gpio_direction_output(gpio_1,0);
        counter_trigger = 1;
        break;

        case 18:
        gpio_1 = 56;
        gpio_2 = 60;
        gpio_3 = 78;
      
        printk("18 \n");
        re = gpio_request(gpio_1,"gpio56_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio60_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio78_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_direction_output(gpio_3,0);
        gpio_set_value(gpio_3,1);
        gpio_direction_output(gpio_2,0);
        gpio_set_value(gpio_2,1);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;

        case 19:
        gpio_1 = 58;
        gpio_2 = 60;
        gpio_3 = 79;
      
        printk("19 \n");
        re = gpio_request(gpio_1,"gpio56_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio60_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio78_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        gpio_direction_output(gpio_3,0);
        gpio_set_value(gpio_3,1);
        gpio_direction_output(gpio_2,0);
        gpio_set_value(gpio_2,1);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 3;
        break;
    }
    trigger_pin = gpio_1;
    return 0;
}

int echo_pin_configure(int echo)            //configuring echo pin
{
    int re;
    switch(echo)
    {
        case 0:
        gpio_5 = 11;
        gpio_6 = 32;
        printk("0 \n");
        re = gpio_request(gpio_5,"gpio11_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio32_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) (irq_handler), IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 2;
        break;

        case 1:
        gpio_5 = 12;
        gpio_6 = 28;
        gpio_7 = 45;
         
        printk("1 \n");
        re = gpio_request(gpio_5,"gpio12_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio28_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio45_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_direction_output(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 2:
        gpio_5 = 13;
        gpio_6 = 34;
        gpio_7 = 77;
         
        printk("2 \n");
        re = gpio_request(gpio_5,"gpio13_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio34_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio45_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 3:
        gpio_5 = 14;
        gpio_6 = 16;
        gpio_7 = 76;
        gpio_8 = 64;
         
        printk("3 \n");
        re = gpio_request(gpio_5,"gpio14_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio34_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio76_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        re = gpio_request(gpio_8,"gpio64_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_8);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_set_value_cansleep(gpio_8,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 4;  
        break;
        
        case 4:
        gpio_5 = 6;
        gpio_6 = 36;
         
        printk("4 \n");
        re = gpio_request(gpio_5,"gpio6_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio36_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 2; 
        break;

        case 5:
        gpio_5 = 0;
        gpio_6 = 18;
        gpio_7 = 66;
         
        printk("5 \n");
        re = gpio_request(gpio_5,"gpio0_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio18_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio66_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 6:
        gpio_5 = 1;
        gpio_6 = 20;
        gpio_7 = 68;
         
        printk("6 \n");
        re = gpio_request(gpio_5,"gpio1_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio20_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio68_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 9:
        gpio_5 = 4;
        gpio_6 = 22;
        gpio_7 = 70;
         
        printk("9 \n");
        re = gpio_request(gpio_5,"gpio4_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio22_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio70_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 10:
        gpio_5 = 10;
        gpio_6 = 26;
        gpio_7 = 74;
         
        printk("10 \n");
        re = gpio_request(gpio_5,"gpio10_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio26_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio74_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 14:
        gpio_5 = 48;
         
        printk("14 \n");
        re = gpio_request(gpio_5,"gpio48_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1;
        break;

        case 15:
        gpio_5 = 50;
         
        printk("15 \n");
        re = gpio_request(gpio_5,"gpio50_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1; 
        break;

        case 16:
        gpio_5 = 52;
         
        printk("16 \n");
        re = gpio_request(gpio_5,"gpio52_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1; 
        break;

        case 17:
        gpio_5 = 54;
         
        printk("17 \n");
        re = gpio_request(gpio_5,"gpio54_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1; 
        break;

        case 18:
        gpio_5 = 56;
        gpio_6 = 60;
        gpio_7 = 78;
         
        printk("18 \n");
        re = gpio_request(gpio_5,"gpio56_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio60_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio78_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 19:
        gpio_5 = 58;
        gpio_6 = 60;
        gpio_7 = 79;
         
        printk("19 \n");
        re = gpio_request(gpio_5,"gpio58_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio60_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio79_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise",  global_struct);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;
    }
    global_struct->echo_pin = gpio_5;
    return 0;
}


static int dotmatrix_probe(struct spi_device *dev_spi) //spi_device's probe function 
{
    int ret = 0;
    spidev_buff = dev_spi;
    printk("entered probe function \n");
    ret = spi_gpio_configure();
    global_struct = kmalloc(sizeof(struct per_device),GFP_KERNEL);
    global_struct->counter_irq = 0;
    measurement_ongoing = 0;
    mutex_init(&(write_mutex));
    mutex_init(&(display_mutex));                                    //initialize lock
    global_struct->worker_thread = kthread_run(get_measurement, NULL,"thread1");               // create a kthread
    mSecDelay = 0;
    display_ready = 0;
    global_struct->display_thread = kthread_run(display_pattern, NULL,"thread2");
    return 0;
}

static int dotmatrix_remove(struct spi_device *dev_spi) //spi_device's remove function
{
    printk("entered remove function of dotmatrix_device \n");
    gpio_free(24);
    gpio_free(72);
    gpio_free(42);
    gpio_free(30);
    gpio_free(46);
    gpio_free(44);
    return 0;
}

static struct spi_driver spidriver_dotmatrix = {
	.driver = {
        .name =	"MAX7219",
		.owner = THIS_MODULE,
	},
	.probe    =	dotmatrix_probe,
	.remove   =	dotmatrix_remove,
	.id_table = device_table,
};

static int receive_message(struct sk_buff* skbuff, struct genl_info* info)
{
    int ret;
    printk("entered receive_message function \n");
    
    if(info->attrs[GENL_STRUCT_ATTR])
    {
        printk("message type is custom structure \n");
        if(patternIter == 0) {
            memcpy(pattern->led1,(struct led_data *)nla_data(info->attrs[GENL_STRUCT_ATTR]),sizeof(struct led_data));
        } else if(patternIter == 1) {
            memcpy(pattern->led2,(struct led_data *)nla_data(info->attrs[GENL_STRUCT_ATTR]),sizeof(struct led_data));
        } else if(patternIter == 2) {
            memcpy(pattern->led3,(struct led_data *)nla_data(info->attrs[GENL_STRUCT_ATTR]),sizeof(struct led_data));
        }
        if(patternIter == 2) {
            mutex_lock(&(display_mutex));
            display_ready = 1;
            mutex_unlock(&(display_mutex));
        }
        patternIter++;
        return 0;
    }
    else if(info->attrs[GENL_STRING_ATTR])
    {
        int cmd_value;
        char *buffer = kmalloc(sizeof(char)*3,GFP_KERNEL);
        char *value = kmalloc(sizeof(char)*4,GFP_KERNEL);

        printk("message type is string \n");
        printk("User space message is %s \n",(char*)nla_data(info->attrs[GENL_STRING_ATTR]));
        strncpy(buffer,(char*)nla_data(info->attrs[GENL_STRING_ATTR]),3);
        printk("Command is = %s \n",buffer);
        strncpy(value,(char*)nla_data(info->attrs[GENL_STRING_ATTR])+4,4);
        printk("Value is = %s \n",value);
        kstrtoint(value,16,&cmd_value);
        ret = strncmp(buffer,"CHS",3);
        if(ret == 0)
        {
            printk("command is chip select \n");
            chipselect_configure(cmd_value);
            if(cs_flag == 0)
            {
                int isDynamic = 0;
                ret = setup_dotmatrix(isDynamic);
                printk("return value of setup_dotmatrix is %d \n",ret);
                cs_flag = 1;
            }
            gpio_set_value_cansleep(global_struct->chip_select,1);
            return 0;
        }
        ret = strncmp(buffer,"DLY",3);
        if(ret == 0)
        {
            mutex_lock(&(display_mutex));
            mSecDelay = cmd_value;
            mutex_unlock(&(display_mutex));
            printk("command is delay : %d \n", mSecDelay);
            return 0;
        }
        ret = strncmp(buffer,"ECH",3);
        if(ret == 0)
        {
            printk("command is echo \n");
            echo_pin_configure(cmd_value);
            return 0;
        }
        ret = strncmp(buffer,"TRG",3);
        if(ret == 0)
        {
            printk("command is trigger \n");
            trigger_pin_configure(cmd_value);
            return 0;
        }
        ret = strncmp(buffer,"DST",3);
        if(ret == 0)
        {
            printk("==> Command is Request Distance \n");
            mutex_lock(&(write_mutex));
            measurement_ongoing = 1;
            mutex_unlock(&(write_mutex));
            return 0;
        }
        kfree(buffer);
        kfree(value);
    }
    return 0;
}

static const struct genl_ops genl_ops[] = {
    {
        .cmd = GENL_TEST_C_MSG,
        .policy = genl_policy,
        .doit = receive_message,
        .dumpit = NULL,
    },
};

static const struct genl_multicast_group genl_mcgrps[] = {
    [genl_mcgrp0] = {.name = genl_mcgp_name} 
};

static struct genl_family family_genl = {
    .name = genl_family_name,
    .version = 1,
    .maxattr = genl_attr_max,
    .netnsok = false,
    .module = THIS_MODULE,
    .ops = genl_ops,
    .n_ops = ARRAY_SIZE(genl_ops),
    .mcgrps = genl_mcgrps,
    .n_mcgrps = ARRAY_SIZE(genl_mcgrps),
};




static int __init setup(void)
{
    int ret;

    ret = spi_register_driver(&spidriver_dotmatrix);
	if(ret < 0) 
	{
        printk("couldn't register spi driver \n"); 
        return -1;
    }

    ret = genl_register_family(&family_genl);
    if(ret)
    {
        printk("Bad generic netlink registration \n");
        return 0;
    }
    pattern = kmalloc(sizeof(struct led_data),GFP_KERNEL);
    return 0;

}


static void __exit cleanup(void)
{
    switch(counter_cs)
    {   
        case 1:
        printk("case 1 counter_cs \n");
        gpio_free(gpio_9);
        break;

        case 2:
        printk("case 2 counter_cs \n");
        gpio_free(gpio_9);
        gpio_free(gpio_10);
        break;

        case 3:
        printk("case 3 counter_cs \n");
        gpio_free(gpio_9);
        gpio_free(gpio_10);
        gpio_free(gpio_11);
        break;

        case 4:
        printk("case 4 counter_cs \n");
        gpio_free(gpio_9);
        gpio_free(gpio_10);
        gpio_free(gpio_11);
        gpio_free(gpio_12);
        break;

    }

    switch(counter_trigger)
    {   
        
        case 1:
        printk("case 1 counter_trigger \n");
        gpio_free(gpio_1);
        break;

        case 2:
        printk("case 2 counter_trigger \n");
        gpio_free(gpio_1);
        gpio_free(gpio_2);
        break;

        case 3:
        printk("case 3 counter_trigger \n");
        gpio_free(gpio_1);
        gpio_free(gpio_2);
        gpio_free(gpio_3);
        break;

        case 4:
        printk("case 4 counter_trigger \n");
        gpio_free(gpio_1);
        gpio_free(gpio_2);
        gpio_free(gpio_3);
        gpio_free(gpio_4);
        break;
    }

    switch(counter_echo)
    {   
        case 1:
        printk("case 1 counter_echo \n");
        gpio_free(gpio_5);
        break;

        case 2:
        printk("case 2 counter_echo \n");
        gpio_free(gpio_5);
        gpio_free(gpio_6);
        break;

        case 3:
        printk("case 3 counter_echo \n");
        gpio_free(gpio_5);
        gpio_free(gpio_6);
        gpio_free(gpio_7);
        break;

        case 4:
        printk("case 4 counter_echo \n");
        gpio_free(gpio_5);
        gpio_free(gpio_6);
        gpio_free(gpio_7);
        gpio_free(gpio_8);
        break;
    }
    kthread_stop(global_struct->worker_thread);
    kthread_stop(global_struct->display_thread);
    kfree(global_struct);
    spi_unregister_driver(&spidriver_dotmatrix);
    genl_unregister_family(&family_genl);
}

module_init(setup);
module_exit(cleanup);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("SHYAM");
MODULE_DESCRIPTION("socket");
