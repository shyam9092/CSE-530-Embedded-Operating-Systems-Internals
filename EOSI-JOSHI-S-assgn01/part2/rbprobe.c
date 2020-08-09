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
#include<linux/semaphore.h>

#define DEV_CLASS	"rbprobecl"
#define DEV_NODE	"RBprobe"

#define KP_SYMBOL_NAME_W "rb_driver_write"
#define KP_SYMBOL_NAME_R "kp_rbtree_sniff"
 
static dev_t first; // Global variable for the first device number 
static struct class *cl; // Global variable for the device class

static struct kprobe *kp_write[50];
static struct kprobe *kp_read[50];
// static int counter_write=0;
// static int counter_read=0;

// static struct semaphore write_lock,read_lock;

struct rb_dev {
	struct cdev cdev;					//kernel's internal structure that represents char devices
	struct rb_root rootobj;				//Initializing root in the character device file to access the tree
	struct rb_node *current_traversal;	//To remember the last node read while traversing the tree in read call
	int read_order;															//To remember the read order (ascending/descending) while traversing the tree in read call
};

struct rb_user{													//Temporary struture to use as a buffer to copy/send data to/from user respectively.
	int key;
	int data;
};


struct probe_data
{
  int pid;
  unsigned long long time_stamp;
  int traverse_count;
  kprobe_opcode_t addr;
};

struct kprobe_data
{
  struct kprobe *kp;
  struct semaphore kp_lock;
  struct probe_data data[50];
  int counter;
};

struct rbprobe_dev
{
  struct cdev c_dev; // Global variable for the character device structure
  struct kprobe_data kprobe_data;
}*global_dev;

typedef struct rb_object{											//Our rbtree structure
	struct rb_node rbnode;											//rbnode to be linked with root's rb_node
	int key;																		//Key through which we will traverse	
	int data;
}rb_traverse;

struct rb_user_buff{    // structure passed from user space
	int select_rw;       // Probe in write if 1 else probe in read
	int flag;           // Register kprobe if 1 else unregister kprobe
  int offset;
};

static unsigned int rb_counter = 0;

int rbtree_search(struct rb_root *root, int key)					//rbtree search function
{
  	struct rb_node *node = (root->rb_node);												//get the root node to start from top of the tree
    int count = 0;
  	while (node)
    {																								//while node has not reached null node (leaf node)
      rb_traverse *this = container_of(node, rb_traverse, rbnode);//get the rb_object structure which has key,data,rbnode 

      if(this->key > key) {																				//compare the given key from user and traverse left/right accordingly
        node = node->rb_left;
        count++;
      }
      else if(this->key < key){
        node = node->rb_right;
        count++;
      }
      else
      {
        printk("Key found %d %d\n", this->key, this->data);						//For debugging purpose
        return count;
      }
	  }
	return 0;
}

int read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{ 
  struct rb_dev *rb_buff = (struct rb_dev *)(regs->ax);
  struct rb_user *rb_user_buff = kmalloc(sizeof(struct rb_user),GFP_KERNEL);
  char *buf =  (char *)(regs->dx);
  int count = (int)regs->cx;
  copy_from_user(rb_user_buff,buf,count);
  down(&global_dev->kprobe_data.kp_lock);
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].time_stamp = native_read_tsc(); 
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].pid = current->pid;
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].addr = *(p->addr);
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].traverse_count = rbtree_search(&rb_buff->rootobj,rb_user_buff->key);

  global_dev->kprobe_data.counter++;
  up(&global_dev->kprobe_data.kp_lock);
  return 0; 
}
 
void read_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{ 
  printk("post handler : rb_counter = %u\n",rb_counter++);
}

int read_fault_handler(struct kprobe *p, struct pt_regs *regs)
{ 
  printk("pre handler : rb_counter = %u\n",rb_counter++);
  return 0; 
}

int write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{ 
  
  struct file *file = (struct file *)(regs->ax);
  struct rb_dev *rb_buff = file->private_data;
  struct rb_user *rb_user_buff = kmalloc(sizeof(struct rb_user),GFP_KERNEL);
  char *buf =  (char *)(regs->dx);
  int count = (int)regs->cx;

  down(&global_dev->kprobe_data.kp_lock);

  copy_from_user(rb_user_buff,buf,count);
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].time_stamp = native_read_tsc(); 
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].pid = current->pid;
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].addr = *(p->addr);
  global_dev->kprobe_data.data[global_dev->kprobe_data.counter].traverse_count = rbtree_search(&rb_buff->rootobj,rb_user_buff->key);

  global_dev->kprobe_data.counter++;

  up(&global_dev->kprobe_data.kp_lock);
  return 0; 
}
 
void write_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{ 
  printk("post handler : rb_counter = %u\n",rb_counter++);
}

int write_fault_handler(struct kprobe *p, struct pt_regs *regs){ 
  printk("pre handler : rb_counter = %u\n",rb_counter++);
  return 0; 
}

static int rbkprobe_open(struct inode *inode, struct file *file)
{
  struct rbprobe_dev *buff = container_of(inode->i_cdev, struct rbprobe_dev,c_dev);

  printk(KERN_INFO "open()\n");

  file->private_data = buff;

  buff->kprobe_data.counter = 0;

  sema_init(&global_dev->kprobe_data.kp_lock,1);

  return 0;
}

static int rbkprobe_close(struct inode *i, struct file *f)
{
  printk(KERN_INFO "close()\n");
  return 0;
}

static ssize_t rbkprobe_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
  struct probe_data *buff_user = kmalloc(sizeof(struct probe_data),GFP_KERNEL);

  printk(KERN_INFO "read()\n");

  down(&global_dev->kprobe_data.kp_lock);

  global_dev->kprobe_data.counter--;

  buff_user->time_stamp = global_dev->kprobe_data.data[global_dev->kprobe_data.counter].time_stamp; 
  buff_user->pid = global_dev->kprobe_data.data[global_dev->kprobe_data.counter].pid;
  buff_user->addr = global_dev->kprobe_data.data[global_dev->kprobe_data.counter].addr;
  buff_user->traverse_count = global_dev->kprobe_data.data[global_dev->kprobe_data.counter].traverse_count;

  copy_to_user(buf,buff_user,sizeof(buff_user));

  up(&global_dev->kprobe_data.kp_lock);
  
  return 0;
}

static ssize_t rbkprobe_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{	
  // struct rbprobe_dev *buff_rbprobe = file->private_data;

  struct rb_user_buff *wr_data = (struct rb_user_buff *)kmalloc(sizeof(struct rb_user_buff),GFP_KERNEL);

  int ret;
  
  printk(KERN_INFO "write()\n");
  
  down(&global_dev->kprobe_data.kp_lock);
	copy_from_user(wr_data,buf,len);

  if(wr_data->select_rw)      // write
  {
    if(wr_data->flag)
    {
      kp_write[global_dev->kprobe_data.counter] = kmalloc(sizeof(struct kprobe),GFP_KERNEL);

      kp_write[global_dev->kprobe_data.counter]->pre_handler = write_pre_handler;
      kp_write[global_dev->kprobe_data.counter]->post_handler = write_post_handler;
      kp_write[global_dev->kprobe_data.counter]->fault_handler = write_fault_handler;
      kp_write[global_dev->kprobe_data.counter]->symbol_name = KP_SYMBOL_NAME_W;
      kp_write[global_dev->kprobe_data.counter]->offset = wr_data->offset;

      ret = register_kprobe(kp_write[global_dev->kprobe_data.counter]);
      global_dev->kprobe_data.counter++;
    }
    else
    {
      unregister_kprobe(kp_write[global_dev->kprobe_data.counter]);
      global_dev->kprobe_data.counter--; 
      kfree(kp_write);
    }
  }
  else                       // read
  {
    if(wr_data->flag)
    {
      kp_read[global_dev->kprobe_data.counter] = kmalloc(sizeof(struct kprobe), GFP_KERNEL);

      kp_read[global_dev->kprobe_data.counter]->pre_handler = read_pre_handler;
      kp_read[global_dev->kprobe_data.counter]->post_handler = read_post_handler;
      kp_read[global_dev->kprobe_data.counter]->fault_handler = read_fault_handler;
      kp_read[global_dev->kprobe_data.counter]->symbol_name = KP_SYMBOL_NAME_R;
      kp_read[global_dev->kprobe_data.counter]->offset = wr_data->offset;
      global_dev->kprobe_data.counter++;
    }
    else
    {
      unregister_kprobe(kp_read[global_dev->kprobe_data.counter]);
      kfree(kp_read[global_dev->kprobe_data.counter]);
      global_dev->kprobe_data.counter--;
    }
  }
  up(&global_dev->kprobe_data.kp_lock);
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

  global_dev = kmalloc(sizeof(struct rbprobe_dev),GFP_KERNEL);

  cdev_init(&global_dev->c_dev, &rbprobe_fops);

  if (cdev_add(&global_dev->c_dev,first,1) == -1) {
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    return -1;
  }

  if (device_create(cl, NULL, first, NULL, DEV_NODE) == NULL) {
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    return -1;
  }

  return 0;
}
 
/* Destructor */
static void __exit rbprobe_exit(void)
{
  cdev_del(&global_dev->c_dev);
  device_destroy(cl, first);
  class_destroy(cl);
  unregister_chrdev_region(first, 1);
  kfree(global_dev);
}
 
module_init(rbprobe_init);
module_exit(rbprobe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SHYAM JOSHI");
MODULE_DESCRIPTION("Kprobe driver");
