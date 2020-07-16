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
#include<linux/kprobes.h>
#include <linux/sched.h>

#define READ_ADDR "dumpstack_read"
#define WRITE_ADDR "dumpstack_write"
#define OPEN_ADDR "dumpstack_open"



#define DEVICE_NAME	  "dumpstack_drv"
#define PROBE_WRITE_FN "dumpstack_write"
#define PROBE_READ_FN "dumpstack_read"

static dev_t dumpstack_dev_number;
struct class *dumpstack_dev_class;	
static struct device *dumpsk_device;


struct kprobe_data
{
    int x;
};

struct dumpstack_dev
{
    struct cdev cdev;
    int key;
    int data; 
    struct kprobe_data *probe_data;
    
}*dumpstack_device;

struct data_user_insert
{
    int dump_mode;
    char *func_name;
};

struct data_user_remove
{
    unsigned int dumpid;
};


typedef int dumpmode_t;
 
struct node
{   
    int dump_mode;
    pid_t parentid;
    struct kprobe *probe;
    unsigned int dumpid;
    struct node *next;
}*head;





int dumpstack_release(struct inode *inode, struct file *file)
{
    printk("entered release call \n");
    printk("leaving successfully \n");
    return 0;
}

ssize_t dumpstack_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    printk("entered write call \n");
    return 0;
}

EXPORT_SYMBOL_GPL(dumpstack_write);

ssize_t dumpstack_read(struct file *file, char *buf, size_t count, loff_t *ppos)  
{
    printk("entered read call \n");
    printk("leaving read call successfully \n");

    return 0;
    
}
EXPORT_SYMBOL_GPL(dumpstack_read);

int dumpstack_open(struct inode *inode, struct file *file)
{
    struct dumpstack_dev *buff;
    printk("entered open call \n");
    buff = (struct dumpstack_dev *)container_of(inode->i_cdev, struct dumpstack_dev, cdev);
    file->private_data = buff;
    printk("leaving open call successfully \n");
    return 0;
}
EXPORT_SYMBOL_GPL(dumpstack_open);

static struct file_operations dumpstack_fops = {    // Fill in the file operation structure with desired file operations
	.owner            =   THIS_MODULE,  	     // and to in	initialize the character device structure with that.
	.open             =   dumpstack_open,
	.release          =   dumpstack_release,
	.write            =   dumpstack_write,
	.read             =   dumpstack_read,
};

int __init dumpstack_init(void)
{
    printk("entered dumpstack_init function \n");
    if(alloc_chrdev_region(&dumpstack_dev_number, 0, 2, DEVICE_NAME) < 0 )    // Dynamic allocation of 32-bit number(Major,Minor)     
	{
		printk(KERN_DEBUG "Couldn't register the device\n"); 
		return -1;
	}
    dumpstack_dev_class = class_create(THIS_MODULE,DEVICE_NAME);
    dumpstack_device  = kmalloc(sizeof(struct dumpstack_dev), GFP_KERNEL);
    cdev_init(&(dumpstack_device->cdev),&dumpstack_fops);
    dumpstack_device->cdev.owner  = THIS_MODULE;
    cdev_add(&(dumpstack_device->cdev),MKDEV(MAJOR(dumpstack_dev_number), 0),1);
    dumpsk_device = device_create(dumpstack_dev_class, NULL, MKDEV(MAJOR(dumpstack_dev_number),0),NULL,DEVICE_NAME);    
    return 0;
}

void __exit dumpstack_exit(void)
{
    printk("entered dumpstack_exit function \n");
    unregister_chrdev_region((dumpstack_dev_number),1);
    device_destroy(dumpstack_dev_class, MKDEV(MAJOR(dumpstack_dev_number),0));
    cdev_del(&(dumpstack_device->cdev));
    kfree(dumpstack_device);
    class_destroy(dumpstack_dev_class);
}

module_init(dumpstack_init);
module_exit(dumpstack_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SHYAM");
MODULE_DESCRIPTION("dumpstack");

