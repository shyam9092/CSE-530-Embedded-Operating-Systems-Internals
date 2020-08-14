#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
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
#include <linux/platform_device.h>
#include "Sample_platform_device.h"


#define DRIVER_NAME		"platform_driver_0"

static int num_devices = 0;

static struct class *HCSR_class;
static dev_t dev_HC=0;

// static struct miscdevice misc_device_persistent[50];

struct config_pins
{
    int trigger_pin;
    int echo_pin;
};

struct set_param
{
    int m;
    int delta;
};

struct write_user 
{
    int write_arg;
};

struct read_buff
{
    unsigned long long distance;
    unsigned long long time;
}*read_buffer;

static int counter_trigger; 
static int counter_echo; 

// static int num = 1;
static int gpio_1;
static int gpio_2;
static int gpio_3;
static int gpio_4;

static int gpio_5;
static int gpio_6;
static int gpio_7;
static int gpio_8;

static int flag_irq = 0;

// static int counter_trigger_global=0;
// static int counter_echo_global=0;

static int irq_num;

static int global_m = 0;

static int flag_wait = 0;

// static int gpio_free_pins[50];
// static int counter_gpio_free = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq);


int err_no;

// module_param(num,int,0000);

enum IOCTL_ARGS 
{
    CONFIG_PINS = 3,
    SET_PARAMETERS = 4
};

static const struct platform_device_id P_id_table[] = {							//used during probe matching of platform devvice
		 { "HCSR_1", 0 },
         { "HCSR_2", 0 },
         { "HCSR_3", 0 },
		 { "HCSR_4", 0 },
		 { "HCSR_5", 0 },
         { "HCSR_6", 0 },
         { "HCSR_7", 0 },
		 { "HCSR_8", 0 },
		 { "HCSR_9", 0 },
         { "HCSR_10", 0 },
         { "HCSR_11", 0 },
		 { "HCSR_12", 0 },
		 { "HCSR_13", 0 },
         { "HCSR_14", 0 },
         { "HCSR_15", 0 },
	 { },
};

int buff_clear(struct hc_sr04driver *fifo)
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

void write_fifo(struct hc_sr04driver *buffer, unsigned long long data_dist, unsigned long long data_time)
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

struct read_buff *read_fifo(struct hc_sr04driver *buffer)
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

void irq_write_fifo(struct hc_sr04driver *irq_write_driver)
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
        time_elapsed[iter] = (irq_write_driver->time_fall[iter] - irq_write_driver->time_rise[iter]);
        time_elapsed[iter] = div_u64(time_elapsed[iter],2);
        time_elapsed_average = time_elapsed_average + time_elapsed[iter];
    }
    time_elapsed_average = div_u64(time_elapsed_average,global_m);
    time_elapsed_average = div_u64(time_elapsed_average,div);
    measurement_done = native_read_tsc();   
    printk("distance = %llu \n",time_elapsed_average);
    printk("time_stamp = %llu \n",measurement_done);
    write_fifo(write_to_fifo,time_elapsed_average,measurement_done);
    printk("done and dusted \n");
}

static int get_measurement(void *args)
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
        if(trigger) 
        {
            for(i=1;i<=m_user;i++) 
            {  
                printk("iteration = %d \n",i);
                gpio_set_value(hc_dev_measure->trigger_pin,1);             // set 1 to trigger pin       
                msleep(1000);
                gpio_set_value(hc_dev_measure->trigger_pin,0);              // trigger for new measurement
                msleep(hc_dev_measure->delta);                              //sleep for delta milliseconds as specified by user
            }
            mutex_lock(&(hc_dev_measure->write_mutex));
            hc_dev_measure->trigger_ongoing_flag = 0;
            mutex_unlock(&(hc_dev_measure->write_mutex));
            irq_write_fifo(hc_dev_measure);
            hc_dev_measure->counter_irq = 0;
        }
    }
    do_exit(0);
    return 0;
}


static irq_handler_t irq_handler(int irq_n, void *dev_id)
{
    struct hc_sr04driver *irq_ptr_hcdriver = (struct hc_sr04driver *)(dev_id);
    
    if(flag_irq == 0)
    {
        irq_ptr_hcdriver->time_rise[irq_ptr_hcdriver->counter_irq] = native_read_tsc();
        irq_set_irq_type(irq_n,IRQ_TYPE_EDGE_FALLING);
        flag_irq  = 1;
    }
    else
    {   
        irq_ptr_hcdriver->time_fall[irq_ptr_hcdriver->counter_irq] = native_read_tsc();
        irq_set_irq_type(irq_n,IRQ_TYPE_EDGE_RISING);
        flag_irq = 0; 
        irq_ptr_hcdriver->counter_irq++;
    }
   return (irq_handler_t) IRQ_HANDLED;
}

int trigger_pin_configure(int trig)         //configuring triggger pin`
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
    // trigger_pin = gpio_1;
    return 0;
}

int echo_pin_configure(int echo, struct hc_sr04driver *global_struct)            //configuring echo pin
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
    // global_struct->echo_pin = gpio_5;
    return 0;
}

static ssize_t trigger_show(struct device *dev,struct device_attribute *attr,char *buf)  			//displays the value of trigger pin
{   
    struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
	printk("trigger_show call \n");
    printk("%d \n",hc_ptr->trigger_pin);
    return 0;  
}


static ssize_t trigger_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)  //stores the value of trigger pin
{
	int argument;
    struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
	sscanf(buf,"%d",&argument);
	printk("trigger_store call \n");
	hc_ptr->trigger_pin = argument;
    trigger_pin_configure(hc_ptr->trigger_pin);
	return 0;
}

static ssize_t echo_show(struct device *dev,struct device_attribute *attr,char *buf) 		//displays the value of echo pin
{   
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);

	printk("echo_show call \n");  
	printk("%d \n",hc_ptr->echo_pin);
	return 0;
}


static ssize_t echo_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)  		//stores the value of trigger pin
{
    int argument;
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
    sscanf(buf,"%d",&argument);
	printk("echo_store call \n");
    hc_ptr->echo_pin = argument;
    echo_pin_configure(hc_ptr->echo_pin,hc_ptr);
    return 0;
}

static ssize_t number_samples_show(struct device *dev,struct device_attribute *attr,char *buf) 	//displays the value of sample
{    
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
	printk("number_samples_show call \n"); 
	printk("%d \n",hc_ptr->m);
	return 0;
}


static ssize_t number_samples_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)  //stores the value of sample
{
    int argument;
    struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
	sscanf(buf,"%d",&argument);
    hc_ptr->m = argument;
    printk("number_samples_store call \n");
    printk("number_sample_stored = %d \n",hc_ptr->m);
	return 0;
}

static ssize_t sampling_period_show(struct device *dev,struct device_attribute *attr,char *buf) //displays the value of sampling period
{    
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
	printk("sampling_period_show call \n");
	printk("sampling_period = %d \n",hc_ptr->delta);
	return 0;
}


static ssize_t sampling_period_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) //stores the value of sampling period
{
    int argument;
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
    sscanf(buf,"%d",&argument);
    hc_ptr->m = argument;
    printk("sampling_period_store call \n");    
	return 0;
}

static ssize_t enable_show(struct device *dev,struct device_attribute *attr,char *buf) //displays the value of enable
{   
    struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
    printk("enable show call \n");
    mutex_lock(&(hc_ptr->write_mutex));
    printk("%d \n",hc_ptr->trigger_ongoing_flag);
    mutex_unlock(&(hc_ptr->write_mutex));
	return 0;	
}


static ssize_t enable_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) //stores the value of enable
{
    int argument;
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
    sscanf(buf,"%d",&argument);
    printk("enable store call \n");

    if(argument)
    {
        if(hc_ptr->echo_pin == -1 || hc_ptr->trigger_pin == -1 || hc_ptr->m == -1 || hc_ptr->delta == -1)
        {
                printk("configurations yet to be received \n");
            return 0;
        }
        mutex_lock(&(hc_ptr->write_mutex));                   
        if(hc_ptr->trigger_ongoing_flag == 0)                 // check either write is ongoing or not
        {
        hc_ptr->trigger_ongoing_flag = 1;               // go for m+2 trigger as no measurement is going on
        printk("trigger on going flag is set \n");
        }        
        mutex_unlock(&(hc_ptr->write_mutex));
    }
    else
    {
        mutex_lock(&(hc_ptr->write_mutex));
        hc_ptr->trigger_ongoing_flag = 0;
        mutex_unlock(&(hc_ptr->write_mutex));
    }
    
    
	return 0;
}

static ssize_t  distance_show(struct device *dev,struct device_attribute *attr,char *buf) //displays the value of distance
{     
    
    struct read_buff *rbuff = kmalloc(sizeof(struct read_buff),GFP_KERNEL);
	struct hc_sr04driver *hc_ptr = 	dev_get_drvdata(dev);
    printk("distance show call \n");
    if(hc_ptr->buff_fifo->counter > 0)                                 // check whether there is data in fifo_buffer
    {
        rbuff = read_fifo(hc_ptr);
        // copy_to_user(buf,rbuff,count);
        printk("Distance = %llu cm \n",rbuff->distance);
        return 0;
    } 
    return 0;  
}

static ssize_t  distance_store(struct device *dev,struct device_attribute *attr,char *buf)  //stores the value of distance
{     
	printk("distance show call \n");
	return 0;  
}


//declare the device attributes
static DEVICE_ATTR(trigger, S_IRUSR | S_IWUSR, trigger_show, trigger_store);
static DEVICE_ATTR(echo, S_IRUSR | S_IWUSR, echo_show, echo_store);
static DEVICE_ATTR(number_samples, S_IRUSR | S_IWUSR, number_samples_show, number_samples_store);
static DEVICE_ATTR(sampling_period, S_IRUSR | S_IWUSR, sampling_period_show, sampling_period_store);
static DEVICE_ATTR(enable, S_IRUSR | S_IWUSR, enable_show, enable_store);
static DEVICE_ATTR(distance, S_IRUSR , distance_show, NULL);

int HC_init(struct P_chip *p, struct hc_sr04driver *hc_driver)   //registers class,create devices and attributes
{	
	int retval;
	if(num_devices==0)			//if the first device is to be created then class needs to be created before that
	{
		printk("Entered Loop\n");
		HCSR_class=class_create(THIS_MODULE,"HCSR");
		if(!HCSR_class)
		{
			printk("Error creating class \n"); 
			return -EFAULT;
	    }
		read_buffer = kmalloc(sizeof(struct read_buff),GFP_KERNEL);
		read_buffer->distance = 0;
		read_buffer->time = 0;
	}
	
	p->HC_dev = device_create(HCSR_class,NULL,dev_HC, NULL, p->name);				//create a device
	dev_HC++;
	retval = device_create_file(p->HC_dev, &dev_attr_trigger);
    if (retval < 0) 
	{
		printk("error : Cannot create trigger show attribute\n");
		return -EFAULT;
    }

	retval = device_create_file(p->HC_dev, &dev_attr_echo);						//create attribute for that device and link them with that device
    if (retval < 0) 
	{
		printk("error : Cannot create trigger show attribute\n");
		return -EFAULT;
    }

	retval = device_create_file(p->HC_dev, &dev_attr_number_samples);
    if (retval < 0) 
	{
		printk("error : Cannot create trigger show attribute\n");
		return -EFAULT;
    }

	retval = device_create_file(p->HC_dev, &dev_attr_sampling_period);
    if (retval < 0) 
	{
		printk("error : Cannot create trigger show attribute\n");
		return -EFAULT;
    }

	retval = device_create_file(p->HC_dev, &dev_attr_enable);
    if (retval < 0) 
	{
		printk("error : Cannot create trigger show attribute\n");
		return -EFAULT;
    }

	retval = device_create_file(p->HC_dev, &dev_attr_distance);
    if (retval < 0) 
	{
		printk("error : Cannot create trigger show attribute\n");
		return -EFAULT;
    }

	
	buff[num_devices] = kmalloc(sizeof(struct fifo_buff),GFP_KERNEL);
	hc_driver->buff_fifo = buff[num_devices];
	hc_driver->buff_fifo->head = 0;
	hc_driver->buff_fifo->tail = 0;
	hc_driver->buff_fifo->empty_fifo = 0;
	hc_driver->buff_fifo->counter = 0;
    hc_driver->trigger_pin = -1;
    hc_driver->echo_pin = -1;
    hc_driver->m = -1;
    hc_driver->delta = -1;

	mutex_init(&(hc_driver->write_mutex));

	hc_driver->counter_irq = 0;

	hc_driver->trigger_ongoing_flag = 0;                                                                      // clear the current write onegoing flag

	hc_driver->worker_thread = kthread_run(get_measurement,hc_driver, "%s", "worker_thread");

	dev_set_drvdata(p->HC_dev,(void *)hc_driver);

	return 0;
}

static int P_driver_probe(struct platform_device *dev_found)
{
	struct P_chip *pchip;
	
	struct hc_sr04driver *hc_buff;
	pchip = container_of(dev_found, struct P_chip, plf_dev);

	hc_buff = container_of(pchip, struct hc_sr04driver, P_chip);
	
	printk(KERN_ALERT "Found the device -- %s  %d \n", pchip->name, pchip->dev_no);

	HC_init(pchip, hc_buff);
	num_devices+=1;
			
	return 0;
};

static int P_driver_remove(struct platform_device *pdev)
{
	
    //device_destroy(HCSR_class, HC_dev);
	class_destroy(HCSR_class);
	return 0;
};

static struct platform_driver P_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= P_driver_probe,
	.remove		= P_driver_remove,
	.id_table	= P_id_table,
};

module_platform_driver(P_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SHYAM");
