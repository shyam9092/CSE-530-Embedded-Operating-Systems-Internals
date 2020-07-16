#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/msr.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <asm/div64.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <asm/delay.h>
#include <linux/stat.h>
#include <linux/threads.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/math64.h>
// #include <errno.h>


static int num = 1;                  // for number of miscellaneous devices to be created
static int gpio_1;                   // local gpio number to be used to free them later       
static int gpio_2;
static int gpio_3;
static int gpio_4;

static int gpio_5;
static int gpio_6;
static int gpio_7;
static int gpio_8;

static int flag_irq = 0;               //used in irq_handler function for rising and falling edge interrupt

static int counter_trigger_global=0;        
static int counter_echo_global=0;

static int irq_num;                     //interrupt number to be requested and later used to request irq 

static int global_m = 0;                //to be used in irq_write_buffer function and is set to m in worker_thread function

static int flag_wait = 0;

static int gpio_free_pins[50];          //array to remember pins to free in exit function 
static int counter_gpio_free = 0;       //counter for above array

static DECLARE_WAIT_QUEUE_HEAD(wq);     //for wait queue in read function


int err_no;                             //indicate error

module_param(num,int,0000);             //take argument from user regarding the number of miscellaneous drivers to be created during insmod command
    
enum IOCTL_ARGS                         //for ioctl command
{
    CONFIG_PINS = 3,
    SET_PARAMETERS = 4
};

struct fifo_buff                        //structure of fifo buffer
{
    int head;
    int tail;
    unsigned long long distance[5];
    unsigned long long time_stamp[5];
    int empty_fifo;
    int counter;
}*buff[30];

struct hc_sr04driver                    //per-device structure
{
    int trigger_ongoing_flag;           //to indicate wether there is on going trigger or not (set by write and read)                       
    int echo_pin;                       
    int trigger_pin;
    int m;
    int delta;
    int counter_irq;                    //for storing time stamp
    unsigned long long time_rise[30];   //used to record time in irq_handler 
    unsigned long long time_fall[30];   //used to record time in irq_handler
    struct miscdevice *misc_device;
    char misc_device_name[30];
    struct fifo_buff *buff_fifo;
    struct mutex write_mutex;           //lock
    struct task_struct *worker_thread;  //worker thread
    int gpio_pins_free[30];             
    int counter_gpio_pins_free;
}*hc_driver[30];

static struct miscdevice misc_device_persistent[50];

struct config_pins                  //used to copy arguments sent by user during ioctl call
{
    int trigger_pin;
    int echo_pin;
};

struct set_param                    //used to copy arguments sent by user during ioctl call
{
    int m;
    int delta;
};

struct write_user                   //used to copy arguments sent by user during write call
{
    int write_arg;
};

struct read_buff                    //used to send arguments (distance and time stamp) to user during read call
{
    unsigned long long distance;
    unsigned long long time;
}*read_buffer;

int buff_clear(struct hc_sr04driver *fifo)          //function to clear data of fifo_buffer (used by write function)
{
    int iter;
    struct hc_sr04driver *fifo_clear = (struct hc_sr04driver *)fifo;
    for(iter=0;iter<=4;iter++)
    {
        fifo_clear->buff_fifo->distance[iter] = 0;
        fifo_clear->buff_fifo->time_stamp[iter] = 0;
    }
    fifo_clear->buff_fifo->head = 0;
    fifo_clear->buff_fifo->tail = 0;
    fifo_clear->buff_fifo->counter = 0;
    fifo_clear->buff_fifo->empty_fifo = 0;
    return 0;
}

void write_fifo(struct hc_sr04driver *buffer, unsigned long long data_dist, unsigned long long data_time)          //fifo_buffer write function
{ 

    struct hc_sr04driver *buff = (struct hc_sr04driver *)buffer;
    buff->buff_fifo->distance[buff->buff_fifo->head] = data_dist;
    buff->buff_fifo->time_stamp[buff->buff_fifo->head] = data_time;
    buff->buff_fifo->head++;
    printk("entered write_fifo \n");
    if(buff->buff_fifo->counter >= 5)
    {
        buff->buff_fifo->tail++;
        if(buff->buff_fifo->tail == 5)
        {
            buff->buff_fifo->tail = 0;
        }
    }
    else
    {
        buff->buff_fifo->counter++;
    }

    if(buff->buff_fifo->head == 5)
    {
        buff->buff_fifo->head = 0;
    }
    if(buff->buff_fifo->empty_fifo == 1)
    {
        buff->buff_fifo->empty_fifo = 0;
    }
    if(flag_wait)
    {
        wake_up(&wq);
    }
}

struct read_buff *read_fifo(struct hc_sr04driver *buffer)                                                          //fifo_buffer read function
{
    struct hc_sr04driver *buff = (struct hc_sr04driver *)buffer;
    if(buff->buff_fifo->empty_fifo == 1) 
    {
        printk("Error no data to read \n");
    }
    else if(buff->buff_fifo->counter <= 0)
    {
        printk("Error no data to read \n");
    }
    else 
    {
        read_buffer->distance = buff->buff_fifo->distance[buff->buff_fifo->tail];
        read_buffer->time = buff->buff_fifo->time_stamp[buff->buff_fifo->tail];
        buff->buff_fifo->tail++;
        buff->buff_fifo->counter--;
        if(buff->buff_fifo->tail == 5)  
        {
            buff->buff_fifo->tail = 0;
        }
        if(buff->buff_fifo->tail == buff->buff_fifo->head)
        {
            buff->buff_fifo->empty_fifo = 1;
        }
        return read_buffer;
    }
    return read_buffer;
}

void irq_write_fifo(struct hc_sr04driver *irq_write_driver)                                       //function used to write to fifo_buffer when all the trigger call is handled
{
    int iter;
    unsigned long long time_elapsed[30];
    unsigned long long time_elapsed_average=0;
    unsigned long long measurement_done;
    unsigned long long div = 11764;
    struct hc_sr04driver *write_to_fifo = (struct hc_sr04driver *)irq_write_driver;
    printk("final stage \n");
    for(iter=0;iter<=global_m;iter++)
    {   
        time_elapsed[iter] = (irq_write_driver->time_fall[iter] - irq_write_driver->time_rise[iter]);       //get the time difference between rising and falling for respective readings
        time_elapsed[iter] = div_u64(time_elapsed[iter],2);
        time_elapsed_average = time_elapsed_average + time_elapsed[iter];
    }
    time_elapsed_average = div_u64(time_elapsed_average,global_m);
    time_elapsed_average = div_u64(time_elapsed_average,div);
    measurement_done = native_read_tsc();                                                   //record time as distance is calculated
    printk("distance = %llu \n",time_elapsed_average);
    printk("time_stamp = %llu \n",measurement_done);
    write_fifo(write_to_fifo,time_elapsed_average,measurement_done);                        //write to fifo_buffer
    printk("done and dusted \n");
}

static int get_measurement(void *args)                                               //worker_thread's function
{
    struct hc_sr04driver *hc_dev_measure = (struct hc_sr04driver *)args;
    int trigger,i,m_user;

    while(!kthread_should_stop()) 
    {
        global_m = (hc_dev_measure->m);
        mutex_lock(&(hc_dev_measure->write_mutex));
        trigger = hc_dev_measure->trigger_ongoing_flag;
        m_user = hc_dev_measure->m;
        mutex_unlock(&(hc_dev_measure->write_mutex));
        m_user = m_user + 2;
        if(trigger)                                                          //check whether trigger_on going flag is set by write function, if yes then trigger m+2 times the trigger pin
        {
            for(i=1;i<=m_user;i++)                                          //trigger m+2 times
            {  
                printk("iteration = %d \n",i);
                gpio_set_value(hc_dev_measure->trigger_pin,1);             // set 1 to trigger pin       
                msleep(1000);
                gpio_set_value(hc_dev_measure->trigger_pin,0);              
                msleep(hc_dev_measure->delta);                              //sleep for delta milliseconds as specified by user
            }
            mutex_lock(&(hc_dev_measure->write_mutex));
            hc_dev_measure->trigger_ongoing_flag = 0;                       //set flag to 0 to indicate that trigger call is completed
            mutex_unlock(&(hc_dev_measure->write_mutex));
            irq_write_fifo(hc_dev_measure);                                 //function call to calculate average and write into fifo_buffer
            for(i=0;i<hc_dev_measure->counter_irq;i++)                      //clear the stored data for further use
            {
                hc_dev_measure->time_rise[hc_dev_measure->counter_irq] = 0; 
                hc_dev_measure->time_fall[hc_dev_measure->counter_irq] = 0;
            }
            hc_dev_measure->counter_irq = 0;
        }
    }
    do_exit(0);
    return 0;
}


static irq_handler_t irq_handler(int irq_n, void *dev_id)               //interrupt handler function
{
    struct hc_sr04driver *irq_ptr_hcdriver = (struct hc_sr04driver *)(dev_id);
    
    if(flag_irq == 0)
    {
        irq_ptr_hcdriver->time_rise[irq_ptr_hcdriver->counter_irq] = native_read_tsc();     //record time for rising edge
        irq_set_irq_type(irq_n,IRQ_TYPE_EDGE_FALLING);                                      //change interrupt type to falling edge 
        flag_irq  = 1;
    }
    else
    {   
        irq_ptr_hcdriver->time_fall[irq_ptr_hcdriver->counter_irq] = native_read_tsc();     //record time for falling edge
        irq_set_irq_type(irq_n,IRQ_TYPE_EDGE_RISING);                                       //change interrupt type to rising edge
        flag_irq = 0; 
        irq_ptr_hcdriver->counter_irq++;
    }
   return (irq_handler_t) IRQ_HANDLED;
}

int configure_gpio_pins(struct hc_sr04driver *ptr_hc_sr04driver,int trigger,int echo)
{
    int re;
    int counter_trigger=0;
    int counter_echo=0;
    struct hc_sr04driver *config_ptr;
    config_ptr = ptr_hc_sr04driver;
    printk("=========INSIDE CONFIG PIN FUNCTION========= \n");
    switch(trigger)
    {
        case 0:
        gpio_1 = 11;
        gpio_2 = 32;
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
        printk("6 \n");
        re = gpio_request(gpio_1,"gpio1_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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

        case 11:
        gpio_1 = 5;
        gpio_2 = 24;
        gpio_3 = 44;
        gpio_4 = 72;
        ptr_hc_sr04driver->trigger_pin = gpio_1;
        printk("11 \n");
        re = gpio_request(gpio_1,"gpio5_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio24_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio44_mux");
        if(re)gpio_request(11,"gpio11");
        {
            printk("Bad GPIO Request %d \n",gpio_3);
            return 0;
            break;
        }
        re = gpio_request(gpio_4,"gpio72_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_4);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_4,0);
        gpio_direction_output(gpio_3,0);
        gpio_direction_output(gpio_2,0);
        gpio_direction_output(gpio_1,0);
        counter_trigger = 4;
        break;
        
        case 12:
        gpio_1 = 15;
        gpio_2 = 42;
        ptr_hc_sr04driver->trigger_pin = gpio_1;
        printk("12 \n");
        re = gpio_request(gpio_1,"gpio15_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio42_dir_out");
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

        case 13:
        gpio_1 = 7;
        gpio_2 = 30;
        gpio_3 = 46;
        ptr_hc_sr04driver->trigger_pin = gpio_1;
        printk("13 \n");
        re = gpio_request(gpio_1,"gpio7_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_2,"gpio30_dir_out");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_3,"gpio46_mux");
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

        case 14:
        gpio_1 = 48;
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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
        ptr_hc_sr04driver->trigger_pin = gpio_1;
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


    switch(echo)
    {
        case 0:
        gpio_5 = 11;
        gpio_6 = 32;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("0 \n");
        re = gpio_request(gpio_5,"gpio11_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        irq_num= gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) (irq_handler), IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("1 \n");
        re = gpio_request(gpio_5,"gpio12_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio28_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio45_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("2 \n");
        re = gpio_request(gpio_5,"gpio13_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio34_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio45_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 4;
        break;
        
        case 4:
        gpio_5 = 6;
        gpio_6 = 36;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("4 \n");
        re = gpio_request(gpio_5,"gpio6_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio36_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("5 \n");
        re = gpio_request(gpio_5,"gpio0_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio18_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio66_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("6 \n");
        re = gpio_request(gpio_5,"gpio1_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio20_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio68_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("9 \n");
        re = gpio_request(gpio_5,"gpio4_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio22_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio70_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("10 \n");
        re = gpio_request(gpio_5,"gpio10_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio26_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio74_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 11:
        gpio_5 = 5;
        gpio_6 = 24;
        gpio_7 = 44;
        gpio_8 = 72;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("11 \n");
        re = gpio_request(gpio_5,"gpio5_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_5);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio24_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_6);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio44_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_7);
            return 0;
            break;
        }
        re = gpio_request(gpio_8,"gpio72_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_8);
            return 0;
            break;
        }
        gpio_set_value_cansleep(gpio_7,0);
        gpio_direction_output(gpio_8,0);
        gpio_direction_output(gpio_6,1);
        gpio_direction_input(gpio_5);
        irq_num = gpio_to_irq(gpio_5);
        if(irq_num < 0)
        {
            printk("bad irq \n");
            return 0;
            break;
        }
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 4;
        break;

        case 12:
        gpio_5 = 15;
        gpio_6 = 42;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("12 \n");
        re = gpio_request(gpio_5,"gpio15_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio42_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 2;
        break;

        case 13:
        gpio_5 = 7;
        gpio_6 = 30;
        gpio_7 = 46;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("13 \n");
        re = gpio_request(gpio_5,"gpio7_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio30_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio46_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;

        case 14:
        gpio_5 = 48;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("14 \n");
        re = gpio_request(gpio_5,"gpio48_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1;
        break;

        case 15:
        gpio_5 = 50;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("15 \n");
        re = gpio_request(gpio_5,"gpio50_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1;
        break;

        case 16:
        gpio_5 = 52;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("16 \n");
        re = gpio_request(gpio_5,"gpio52_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 1;
        break;

        case 17:
        gpio_5 = 54;
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("17 \n");
        re = gpio_request(gpio_5,"gpio54_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("18 \n");
        re = gpio_request(gpio_5,"gpio56_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio60_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio78_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
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
        ptr_hc_sr04driver->echo_pin = gpio_5;
        printk("19 \n");
        re = gpio_request(gpio_5,"gpio58_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_1);
            return 0;
            break;
        }
        re = gpio_request(gpio_6,"gpio60_dir_in");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_2);
            return 0;
            break;
        }
        re = gpio_request(gpio_7,"gpio79_mux");
        if(re)
        {
            printk("Bad GPIO Request %d \n",gpio_3);
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
        re = request_irq(irq_num, (irq_handler_t) irq_handler, IRQF_TRIGGER_RISING, "rise", ptr_hc_sr04driver);
        if(re < 0)
        {
            printk("Bad request_irq \n");
        }
        counter_echo = 3;
        break;
    }
    printk("exiting configure_gpio_pins \n"); 
    counter_echo_global = counter_echo;
    counter_trigger_global = counter_trigger;
    
    switch(counter_trigger)
    {
        case 1:
        gpio_free_pins[counter_gpio_free]=gpio_1;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_1;
        config_ptr->counter_gpio_pins_free++;
        counter_gpio_free++;
        break;

        case 2:
        gpio_free_pins[counter_gpio_free]=gpio_1;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_2;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_1;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_2;
        config_ptr->counter_gpio_pins_free++;
        break;

        case 3:
        gpio_free_pins[counter_gpio_free]=gpio_1;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_2;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_3;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_1;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_2;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_3;
        config_ptr->counter_gpio_pins_free++;
        break;

        case 4:
        gpio_free_pins[counter_gpio_free]=gpio_1;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_2;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_3;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_4;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_1;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_2;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_3;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_4;
        config_ptr->counter_gpio_pins_free++;
        break;
    }
    switch(counter_echo)
    {
        case 1:
        gpio_free_pins[counter_gpio_free]=gpio_5;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_5;
        config_ptr->counter_gpio_pins_free++;
        break;

        case 2:
        gpio_free_pins[counter_gpio_free]=gpio_5;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_6;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_5;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_6;
        config_ptr->counter_gpio_pins_free++;
        break;

        case 3:
        gpio_free_pins[counter_gpio_free]=gpio_5;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_6;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_7;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_5;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_6;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_7;
        config_ptr->counter_gpio_pins_free++;
        break;

        case 4:
        gpio_free_pins[counter_gpio_free]=gpio_5;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_6;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_7;
        counter_gpio_free++;
        gpio_free_pins[counter_gpio_free]=gpio_8;
        counter_gpio_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_5;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_6;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_7;
        config_ptr->counter_gpio_pins_free++;
        config_ptr->gpio_pins_free[config_ptr->counter_gpio_pins_free] = gpio_8;
        config_ptr->counter_gpio_pins_free++;
        break;
    }

    printk("counter_trig_globabl = %d \t counter_ech_global = %d \n",counter_trigger_global,counter_echo_global);
    printk("=========EXIT CONFIG PIN FUNCTION========= \n");
    return 1;
}

int hcsr_driver_open(struct inode *inode, struct file *file)
{
    int minor_open,iter=1;
    printk("entered open \n");
    minor_open = iminor(inode);                                     ///get the minor number from inode 
    while(iter<=num)                                                //get the per-device structure from the minor number of the miscellaneous driver
    {   
        if(minor_open == hc_driver[iter]->misc_device->minor)
        {   
            printk("minor number found: %d \n",hc_driver[iter]->misc_device->minor);
            printk("--------------------BYE BYE-------------------------- \n");
            break;
        }
        else
        {
            iter = iter + 1;        
        }

    }
    printk("minor number found for user space open call = %d \n",hc_driver[iter]->misc_device->minor);
    file->private_data = hc_driver[iter];                           //update private_data with the address of the per-device structure for future use
    return 0;
}

int hcsr_driver_release(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t hcsr_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    int re;
    struct write_user *write;
    struct hc_sr04driver *hc_dev_write = file->private_data;
    write = kmalloc(sizeof(struct write_user),GFP_KERNEL);
    copy_from_user(write,buf,count);
    printk("write value = %d \n",write->write_arg);
    if(write->write_arg != 0) {                                 //if value is not zero then clear the buffer
        printk("clear buffer data \n");
        re = buff_clear(hc_dev_write);
    } 
    else                                                        //else go for trigger call    
    {
        mutex_lock(&(hc_dev_write->write_mutex));                   //use lock before checking trigger_on_going_flag as worker_thread can update it any time
        if(hc_dev_write->trigger_ongoing_flag == 0)                 // check either write is ongoing or not
        {
            hc_dev_write->trigger_ongoing_flag = 1;               // go for m+2 trigger as no measurement is going on
            printk("trigger on going flag is set \n");
        }
        else
        {
            return -EINVAL;
        }        
        mutex_unlock(&(hc_dev_write->write_mutex));
    
        
    }
    return 0;
}

ssize_t hcsr_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    struct hc_sr04driver *hc_dev = file->private_data;
    struct read_buff *rbuff = kmalloc(sizeof(struct read_buff),GFP_KERNEL);
    if(hc_dev->buff_fifo->counter > 0)                                 // check whether there is data in fifo_buffer
    {
        rbuff = read_fifo(hc_dev);                                     //read the data stored in the buffer
        copy_to_user(buf,rbuff,count);
        return 0;
    } 
    else                                                                //no data to read then either check there is on going measurement or trigger measurement
    {
        mutex_lock(&(hc_dev->write_mutex));                             //lock before checking trigger_ongoing_flag as worker thread can update it anytime
        if(hc_dev->trigger_ongoing_flag == 0)                           // check either write is ongoing or not, if not then set the trigger_ongoing_flag to 1 for worker thread to trigger 
        {
            hc_dev->trigger_ongoing_flag = 1;                           // got for m+2 trigger as no measurement is going on
        }
        mutex_unlock(&(hc_dev->write_mutex));
        flag_wait = 1;
        wait_event_interruptible(wq, hc_dev->buff_fifo->counter >= 1);  // sleep (wait) till there is data in fifo_buffer 
        rbuff = read_fifo(hc_dev);                                      // read the data    
        copy_to_user(buf,rbuff,count);                                  // send the data to user
        flag_wait = 0;
        return 0;
    }
    kfree(rbuff);

    return 0;
}


long int hcsr_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int re;
    int trig_buf,echo_buf;
    int flag_trig,flag_echo;
    struct hc_sr04driver *driver_ioctl;
    struct set_param *param_ioctl;
    struct config_pins *config_ioctl;

    driver_ioctl = file->private_data;
    mutex_lock(&(driver_ioctl->write_mutex));
    if(driver_ioctl->trigger_ongoing_flag == 1)                             //use lock before checking if there is on going measurement or not
    {
        mutex_unlock(&(driver_ioctl->write_mutex));                        //unlock the lock and return 0 as there is on going measurement
        return 0;
    } 
    mutex_unlock(&(driver_ioctl->write_mutex));                             
    config_ioctl = kmalloc(sizeof(struct config_pins),GFP_KERNEL);          //buffer to copy the trigger and echo pins from user
    param_ioctl  = kmalloc(sizeof(struct set_param),GFP_KERNEL);            //buffer to copy m and delta from user

    printk("entered ioctl function \n");

    if(cmd == CONFIG_PINS)                                                  //if user has passed configure_pins command
    {
        
        copy_from_user(config_ioctl,(struct config_pins *)arg,sizeof(struct config_pins));      //copy echo pin number and trigger pin number from user's argument
        printk("trigger_pin = %d \t echo_pin = %d \n",config_ioctl->trigger_pin,config_ioctl->echo_pin);
        trig_buf = config_ioctl->trigger_pin;
        echo_buf = config_ioctl->echo_pin;
        if(echo_buf >=0 || echo_buf <=6 || echo_buf >=9 || echo_buf <=16)               //check if the echo pin are valid or not 
        {
            flag_echo = 1;
        }
        if(trig_buf >=0 || trig_buf <=16)                                               //check if trigger pin are valid or not
        {
            flag_trig = 1;
        }
        if(flag_trig  == 1 && flag_echo == 1 && (echo_buf != trig_buf))                   //if both echo pin and trigger pin are valid then configure the corresponding gpio pins
        {
            re = configure_gpio_pins(driver_ioctl,trig_buf,echo_buf);                      
            printk("return value of configure_gpio_pins = %d \n",re);            
        }
        else
        {
            printk("Bad pin \n");
            return -EINVAL;
        }
    } 
    else if(cmd == SET_PARAMETERS)                                  // if the user has passed set_parameters command
    {        
        copy_from_user(param_ioctl,(struct set_param *)arg,sizeof(struct set_param));     //copy m and delta from user's argument to a temporary buffer
        printk("entered SET_PARAMETER CALL \n");
        printk("m = %d \t delta = %d \n",param_ioctl->m,param_ioctl->delta); 
        if(param_ioctl->delta >= 60)                                                    //check if delta is >= 60 (as suggested in data sheet of hc_sr04driver to keep an interval of atleast 60ms)
        {
            mutex_lock(&(driver_ioctl->write_mutex));
            driver_ioctl->delta = param_ioctl->delta;                           //update the per-device structure's delta member
            mutex_unlock(&(driver_ioctl->write_mutex)); 
        }
        else
        {
            mutex_lock(&(driver_ioctl->write_mutex));
            driver_ioctl->delta = 60; 
            mutex_unlock(&(driver_ioctl->write_mutex)); 
        }
        
        mutex_lock(&(driver_ioctl->write_mutex));
        driver_ioctl->m = param_ioctl->m;                                       //update the per-device structure's m member
        mutex_unlock(&(driver_ioctl->write_mutex));        
    }
    else
    {
        printk("-------------------------------Bad IOCTL--------------------------------------------- \n");
    }
    kfree(config_ioctl);
    config_ioctl=NULL;
    kfree(param_ioctl);
    param_ioctl=NULL;
    printk("exiting ioctl \n");
    return 0;
}

static struct file_operations hcsr_fops = {
    .owner		= THIS_MODULE,              /* Owner */
    .open		= hcsr_driver_open,         /* Open method */
    .release	= hcsr_driver_release,      /* Release method */
    .write		= hcsr_driver_write,        /* Write method */
    .read		= hcsr_driver_read,         /* Read method */
    .unlocked_ioctl = hcsr_driver_ioctl,
};

int __init hcsr_driver_init(void)
{   
    int ret,i=0; 
    printk("entered init function \n");
	
    for(i=1;i<=num;i++) 
    {   
        hc_driver[i] = kmalloc(sizeof(struct hc_sr04driver),GFP_KERNEL);                //allocate kernel memory for our per-device structure
        buff[i] = kmalloc(sizeof(struct fifo_buff),GFP_KERNEL);                         //allocate memory for fifo_buffer
        sprintf(hc_driver[i]->misc_device_name, "HCSR_%d",i);                           //allocate HCSR_n name as per the iteration variable of for loop
        misc_device_persistent[i].minor = MISC_DYNAMIC_MINOR;                           //dynamically allocate a minor number for miscellaneous device  
        misc_device_persistent[i].fops = &hcsr_fops;                                    //link file operations         
        misc_device_persistent[i].name = hc_driver[i]->misc_device_name;                
        hc_driver[i]->misc_device = &misc_device_persistent[i];                          //link per-device miscellaneous device 
        hc_driver[i]->buff_fifo = buff[i];                                               //initialize values of fifo_bufffer
        hc_driver[i]->buff_fifo->head = 0;
        hc_driver[i]->buff_fifo->tail = 0;
        hc_driver[i]->buff_fifo->empty_fifo = 0;
        hc_driver[i]->buff_fifo->counter = 0;
        printk("misc-device.name  = %s\n", hc_driver[i]->misc_device->name);
        
        ret = misc_register(hc_driver[i]->misc_device);                             //register miscellaneous driver with kernel
        if(ret==0) {
            printk("successfully registered and minor number for hscr_[%d] is :%d\n",i,hc_driver[i]->misc_device->minor);
        } else {
            printk("Error in registering miscellaneous driver number %d \n",i);   
            return 0;        
        }

        mutex_init(&(hc_driver[i]->write_mutex));                                    //initialize lock  
        
        hc_driver[i]->counter_irq = 0;                                               

        hc_driver[i]->trigger_ongoing_flag = 0;                                       // clear the current write onegoing flag

        hc_driver[i]->worker_thread = kthread_run(get_measurement,hc_driver[i], "%s", "worker_thread");               // create a kthread
    }
    read_buffer = kmalloc(sizeof(struct read_buff),GFP_KERNEL);
    read_buffer->distance = 0;
    read_buffer->time = 0;
    printk("exiting init \n");
    return 0;
}

/* Driver Exit */
void __exit hcsr_driver_exit(void)
{   
    int i;
    //int j;

    // for(i=0;i<=num;i++)                                                 //free gpio_pins 
    // {
    //     if(hc_driver[i]->counter_gpio_pins_free >=1)    
    //     {
    //         for(j=0;j<hc_driver[i]->counter_gpio_pins_free;j++)
    //         {
    //             gpio_free(hc_driver[i]->gpio_pins_free[j]);
    //         }
    //     }    
    // }
    
    
    for(i=0;i<counter_gpio_free;i++)
    {
        printk("gpio pin number = %d \n",gpio_free_pins[i]);
        gpio_free(gpio_free_pins[i]);
    }

  


    kfree(read_buffer);                         //free the temporary read buffer used to send data to user

    for(i=1;i<=num;i++) 
    {   
        kfree(buff[i]);                                         //free the memory allocated to fifo_buffer of per-device structure
        kthread_stop(hc_driver[i]->worker_thread);              //stop the kthread 
        misc_deregister(hc_driver[i]->misc_device);             //deregister miscellaneous device
        kfree(hc_driver[i]);                                    //free the memory allocated to per-device structure  
        printk("misc_driver number %d removed successfully \n",i);
    }
}

module_init(hcsr_driver_init);
module_exit(hcsr_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHYAM");
MODULE_DESCRIPTION("HC_SR04 driver");