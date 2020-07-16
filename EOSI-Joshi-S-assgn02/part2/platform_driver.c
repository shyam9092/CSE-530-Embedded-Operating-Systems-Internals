#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "Sample_platform_device.h"

#define DRIVER_NAME		"platform_driver_0"

static int num_devices = 0;

static struct class *HCSR_class;
static dev_t dev_HC=0;

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


static ssize_t trigger_show(struct device *dev,struct device_attribute *attr,char *buf)  			//displays the value of trigger pin
{   
	printk("trigger_show call \n");
	return 0;  
}


static ssize_t trigger_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)  //stores the value of trigger pin
{
	printk("trigger_store call \n");
	return 0;
}

static ssize_t echo_show(struct device *dev,struct device_attribute *attr,char *buf) 		//displays the value of echo pin
{   
	printk("echo_show call \n");  
	return 0;
}


static ssize_t echo_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)  		//stores the value of trigger pin
{
	printk("echo_store call \n");
	return 0;
}

static ssize_t number_samples_show(struct device *dev,struct device_attribute *attr,char *buf) 	//displays the value of sample
{    
	printk("number_samples_show call \n"); 
	return 0;
}


static ssize_t number_samples_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)  //stores the value of sample
{
	printk("number_samples_store call \n");
	return 0;
}

static ssize_t sampling_period_show(struct device *dev,struct device_attribute *attr,char *buf) //displays the value of sampling period
{     
	printk("sampling_period_show call \n");
	return 0;
}


static ssize_t sampling_period_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) //stores the value of sampling period
{
	printk("sampling_period_store call \n");
	return 0;
}

static ssize_t  enable_show(struct device *dev,struct device_attribute *attr,char *buf) //displays the value of enable
{     
	printk("enable show call \n");
	return 0;	
}


static ssize_t enable_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) //stores the value of enable
{
	printk("enable store call \n");
	return 0;
}

static ssize_t  distance_show(struct device *dev,struct device_attribute *attr,char *buf) //displays the value of distance
{     
	printk("distance show call \n");
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

int HC_init(struct P_chip* p)   //registers class,create devices and attributes
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

	return 0;
	
	
}

static int P_driver_probe(struct platform_device *dev_found)
{
	struct P_chip *pchip;
	
	pchip = container_of(dev_found, struct P_chip, plf_dev);
	
	printk(KERN_ALERT "Found the device -- %s  %d \n", pchip->name, pchip->dev_no);

	HC_init(pchip);
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
