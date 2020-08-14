#include <linux/platform_device.h>

#ifndef __SAMPLE_PLATFORM_H__


  
#define __SAMPLE_PLATFORM_H__


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
    unsigned long long time_fall[30];
    struct miscdevice *misc_device;
    char misc_device_name[30];
    struct fifo_buff *buff_fifo;
    struct mutex write_mutex;
    struct task_struct *worker_thread;
    int gpio_pins_free[30];
    int counter_gpio_pins_free;
	struct P_chip P_chip;
}*hc_driver[30];






#endif /* __GPIO_FUNC_H__ */
