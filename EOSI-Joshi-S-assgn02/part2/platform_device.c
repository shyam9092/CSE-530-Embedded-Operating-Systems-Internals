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
#include <linux/platform_device.h>

static int num = 1;
// static int gpio_1;
// static int gpio_2;
// static int gpio_3;
// static int gpio_4;

// static int gpio_5;
// static int gpio_6;
// static int gpio_7;
// static int gpio_8;

// static int flag_irq = 0;

// static int counter_trigger_global=0;
// static int counter_echo_global=0;

// static int irq_num;

// static int global_m = 0;

// static int flag_wait = 0;

// static int gpio_free_pins[50];
// static int counter_gpio_free = 0;

// static DECLARE_WAIT_QUEUE_HEAD(wq);


int err_no;

module_param(num,int,0000);

enum IOCTL_ARGS 
{
    CONFIG_PINS = 3,
    SET_PARAMETERS = 4
};



struct fifo_buff
{
    int head;
    int tail;
    unsigned long long distance[5];
    unsigned long long time_stamp[5];
    int empty_fifo;
    int counter;
}*buff[30];

struct P_chip {
	char *name;
	int	dev_no;
	struct platform_device plf_dev;
    struct device *HC_dev;
};

struct hc_sr04driver
{
    int trigger_ongoing_flag;
    int irq_flag;
    int echo_pin;
    int trigger_pin;
    int m;
    int delta;
    int counter_irq;
    unsigned long long time_rise[30];
    struct miscdevice *misc_device;
    char misc_device_name[30];
    struct fifo_buff *buff_fifo;
    struct mutex write_mutex;
    struct task_struct *worker_thread;
    int gpio_pins_free[30];
    int counter_gpio_pins_free;
	struct P_chip P_chip;
}*hc_driver[30];

static struct miscdevice misc_device_persistent[50];

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

struct hc_sr04driver *hc_driver[30];

int __init hcsr_driver_init(void)
{   
    int i=0; 
    printk("entered init function \n");
	
    for(i=1;i<=num;i++) 
    {   
        hc_driver[i] = kmalloc(sizeof(struct hc_sr04driver),GFP_KERNEL);
        // buff[i] = kmalloc(sizeof(struct fifo_buff),GFP_KERNEL);
        sprintf(hc_driver[i]->misc_device_name, "HCSR_%d",i);
        misc_device_persistent[i].minor = MISC_DYNAMIC_MINOR;
        // misc_device_persistent[i].fops = &hcsr_fops;
        misc_device_persistent[i].name = hc_driver[i]->misc_device_name;
        // hc_driver[i]->misc_device = &misc_device_persistent[i];
		hc_driver[i]->P_chip.name = hc_driver[i]->misc_device_name;
		hc_driver[i]->P_chip.dev_no = i;
		hc_driver[i]->P_chip.plf_dev.name = hc_driver[i]->misc_device_name;
		hc_driver[i]->P_chip.plf_dev.id = -1;
        // hc_driver[i]->buff_fifo = buff[i];
        // hc_driver[i]->buff_fifo->head = 0;
        // hc_driver[i]->buff_fifo->tail = 0;
        // hc_driver[i]->buff_fifo->empty_fifo = 0;
        // hc_driver[i]->buff_fifo->counter = 0;
        platform_device_register(&(hc_driver[i]->P_chip.plf_dev));
        // ret = misc_register(hc_driver[i]->misc_device);
        // if(ret==0)
        // {
        //     printk("successfully registered and minor number for hscr_[%d] is :%d\n",i,hc_driver[i]->misc_device->minor);
        // } 
        // else
        // {
        //     printk("Error in registering miscellaneous driver number %d \n",i);   
        //     return 0;        
        // }

        mutex_init(&(hc_driver[i]->write_mutex));
        
        hc_driver[i]->counter_irq = 0;

        hc_driver[i]->trigger_ongoing_flag = 0;                                                                      // clear the current write onegoing flag

        // hc_driver[i]->worker_thread = kthread_run(get_measurement,hc_driver[i], "%s", "worker_thread");               // create a kthread
    }
    // read_buffer = kmalloc(sizeof(struct read_buff),GFP_KERNEL);
    // read_buffer->distance = 0;
    // read_buffer->time = 0;
    printk("exiting init \n");
    return 0;
}

/* Driver Exit */
void __exit hcsr_driver_exit(void)
{   
    int i;

    // for(i=0;i<counter_gpio_free;i++)
    // {
    //     printk("gpio pin number = %d \n",gpio_free_pins[i]);
    //     gpio_free(gpio_free_pins[i]);
    // }


    // kfree(read_buffer);

    for(i=1;i<=num;i++) 
    {   
        // kfree(buff[i]);
        // kthread_stop(hc_driver[i]->worker_thread);
        platform_device_unregister(&hc_driver[i]->P_chip.plf_dev);
		// misc_deregister(hc_driver[i]->misc_device);
        kfree(hc_driver[i]);
        printk("misc_driver number %d removed successfully \n",i);
    }
}

module_init(hcsr_driver_init);
module_exit(hcsr_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHYAM");
MODULE_DESCRIPTION("HC_SR04 driver");




