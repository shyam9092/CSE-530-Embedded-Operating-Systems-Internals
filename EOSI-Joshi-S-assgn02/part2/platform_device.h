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


struct fifo_buff
{
    int head;
    int tail;
    unsigned long long distance[5];
    unsigned long long time_stamp[5];
    int empty_fifo;
    int counter;
}*buff[30];

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
    unsigned long long time_fall[30];
    struct miscdevice *misc_device;
    char misc_device_name[30];
    struct fifo_buff *buff_fifo;
    struct mutex write_mutex;
    struct task_struct *worker_thread;
    int gpio_pins_free[30];
    int counter_gpio_pins_free;
}*hc_driver[30];


struct HCSR_devices {
	char *name;
	int	dev_no;
	struct platform_device plf_dev;
};