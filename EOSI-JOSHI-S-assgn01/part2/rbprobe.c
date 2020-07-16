#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>

#define DEV_CLASS	"rbprobecl"
#define DEV_NODE	"RBprobe"
 
static dev_t first; // Global variable for the first device number 
static struct cdev c_dev; // Global variable for the character device structure
static struct class *cl; // Global variable for the device class

static struct kprobe kp;

struct rb_user_buff{
	char *buff;
	int flag;
};

static unsigned int rb_counter = 0;

int rbkprobe_pre_handler(struct kprobe *p, struct pt_regs *regs){ 
    printk("pre handler : rb_counter = %u\n",rb_counter++);
    return 0; 
}
 
void rbkprobe_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags){ 
    printk("post handler : rb_counter = %u\n",rb_counter++);
}

int rbkprobe_fault_handler(struct kprobe *p, struct pt_regs *regs){ 
    printk("pre handler : rb_counter = %u\n",rb_counter++);
    return 0; 
}

static int rbkprobe_open(struct inode *i, struct file *f)
{
  printk(KERN_INFO "open()\n");
  return 0;
}

static int rbkprobe_close(struct inode *i, struct file *f)
{
  printk(KERN_INFO "close()\n");
  return 0;
}

static ssize_t rbkprobe_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
  printk(KERN_INFO "read()\n");

  // dump all probed data into the buff

  // copy to user
  
  return 0;
}

static ssize_t rbkprobe_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{	
  struct rb_user_buff *wr_data = kmalloc(sizeof(struct rb_user_buff),GFP_KERNEL);
  printk(KERN_INFO "write()\n");
  
  // copy from user to take data from use about kprobe configuration
	copy_from_user(wr_data,buf,len);
  // check if flag is 0 or 1
  if(wr_data->flag == 1) {
	  // update the probed address to probe the data
	  kp.addr = wr_data->buff;

	  // to register the kprobe
	  register_kprobe(&kp);
  } else {
    // to unregister the kprobe
    unregister_kprobe(&kp);
  }

  return len;
}

static struct file_operations rbprobe_fops =
{
  .owner =			THIS_MODULE,
  .open =				rbkprobe_open,
  .release =		rbkprobe_close,
  .read =				rbkprobe_read,
  .write =			rbkprobe_write
};
 
/* Constructor */
static int __init rbprobe_init(void)
{
  if (alloc_chrdev_region(&first, 0, 1, DEV_CLASS) < 0) {
    return -1;
  }
  
  if ((cl = class_create(THIS_MODULE, DEV_CLASS)) == NULL) {
    unregister_chrdev_region(first, 1);
    return -1;
  }
  
  if (device_create(cl, NULL, first, NULL, DEV_NODE) == NULL) {
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    return -1;
  }
  
  cdev_init(&c_dev, &rbprobe_fops);	
  
  if (cdev_add(&c_dev, first, 1) == -1) {
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    return -1;
  }
  
  kp.pre_handler = rbkprobe_pre_handler;
  kp.post_handler = rbkprobe_post_handler;
  // kp.fault_handler = rbkprobe_fault_handler;

  return 0;
}
 
/* Destructor */
static void __exit rbprobe_exit(void)
{
  cdev_del(&c_dev);
  device_destroy(cl, first);
  class_destroy(cl);
  unregister_chrdev_region(first, 1);
}
 
module_init(rbprobe_init);
module_exit(rbprobe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SHYAM JOSHI");
MODULE_DESCRIPTION("Kprobe driver");
