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

#define DEVICE_NAME		"rbt530_dev"
#define DEVICE_NAME_1 "rbt530_dev1"    //Device name that is to be registered in kernel
#define DEVICE_NAME_2 "rbt530_dev2"


struct rb_dev {
	struct cdev cdev;					//kernel's internal structure that represents char devices
	struct rb_root rootobj;				//Initializing root in the character device file to access the tree
	struct rb_node *current_traversal;	//To remember the last node read while traversing the tree in read call
	int read_order;															//To remember the read order (ascending/descending) while traversing the tree in read call
} *rb_device, *rb_device2;

typedef struct rb_object{											//Our rbtree structure
	struct rb_node rbnode;											//rbnode to be linked with root's rb_node
	int key;																		//Key through which we will traverse	
	int data;
}rb_traverse;

struct rb_user_buff{													//Temporary struture to use as a buffer to copy/send data to/from user respectively.
	int key;
	int data;
};

static dev_t rb_dev_number;										//For device number (Major, Minor) 32-bit number
struct class *rb_dev_class;										//For Kernel to populate appropriate device class
static struct device *rb_dev_device, *rb_dev_device2;	// At lowest level, every device in a linux system is represented by an instance of struct device, it contains info that device model core needs  


// function to set the device default permission for my laptop
/*
static char *set_devnode(struct device *dev, umode_t *mode)
{
  if (!mode)
    return NULL;
  if (dev->devt == MKDEV(MAJOR(rb_dev_number), 0) ||
            dev->devt == MKDEV(MAJOR(rb_dev_number), 1))
    *mode = 0666;
  return NULL;
}  */

rb_traverse *rbtree_search(struct rb_root *root, int key)					//rbtree search function
{
  	struct rb_node *node = (root->rb_node);												//get the root node to start from top of the tree

  	while (node) {																								//while node has not reached null node (leaf node)
  		rb_traverse *this = container_of(node, rb_traverse, rbnode);//get the rb_object structure which has key,data,rbnode 

		if (this->key > key)																					//compare the given key from user and traverse left/right accordingly
  			node = node->rb_left;
		else if (this->key < key)
  			node = node->rb_right;
		else {
			printk("Key found %d %d\n", this->key, this->data);						//For debugging purpose
			return this;
		}
	}
	return NULL;
}

int rbtree_insert(struct rb_root *root, rb_traverse *new_object) //rbtree insert function
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;				//Initialize double pointer(link) to access rbnode as well as structure rb_object
	if(new_object->data) {																					// if data value is non-zero then add the node to the device tree
		while (*new) {																								// find the right leaf node for the new node where to insert
			rb_traverse *this = rb_entry(*new, rb_traverse, rbnode);
			parent = *new;
			if (this->key > new_object->key) {	
				new = &((*new)->rb_left);
			} else if (this->key < new_object->key) {
				new = &((*new)->rb_right);
			} else if (this->key == new_object->key) {
				this->data = new_object->data;
				return 0;
			} else {
				return -1;
			}
		}

		rb_link_node(&new_object->rbnode, parent, new);             	// Add the new node to location we found from above and rebalance tree.
		rb_insert_color(&new_object->rbnode, root);										// Add the appropriate red or black color to the new node
	} else {																												// if data value is zero than remove the node with the same key value 
		rb_traverse *this = rbtree_search(root, new_object->key);			// Search if the tree has node with same key as provided by the user
		rb_erase(&this->rbnode, root);																// delete this node if key is found
	}

	return 0;
}

int rb_driver_open(struct inode *inode, struct file *file)
{
	struct rb_dev *rb_device_open;

	rb_device_open = container_of(inode->i_cdev, struct rb_dev, cdev);  
	
	file->private_data = rb_device_open;														//Assign the address of our character device structure
	return 0;
}

int rb_driver_release(struct inode *inode, struct file *file)
{
	// struct rb_dev *rb_device_release = file->private_data;
	return 0;
}

ssize_t rb_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int status;
	struct rb_dev *rb_device_write = file->private_data;        //Get the address of our char device structure
	rb_traverse *rb_insert_data;                                //temporary node structure to insert the incoming node from user and pass the node to our insert function
	struct rb_user_buff *write_buff = kmalloc(sizeof(struct rb_user_buff),GFP_KERNEL);  //Temporary buffer created to store the key and data provided by user
		
	copy_from_user(write_buff, buf, count);                     	// Copy the data from the user
	
	rb_insert_data = kmalloc(sizeof(rb_traverse),GFP_KERNEL);     //Put the copied data into our temproary structure for insert function
	rb_insert_data->key  = write_buff->key;
	rb_insert_data->data = write_buff->data;
	
	status = rbtree_insert(&(rb_device_write->rootobj),rb_insert_data);
	printk("insert = %d\n", status);															//Check the status of insert call

	return 0;
}

ssize_t rb_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)          //Read function
{
	rb_traverse *rb_read_data;																															//Temporary node structure
	struct rb_user_buff *read_buff;																													//Temporary buffer structure to pass the key and data to the user
	struct rb_dev *rb_device = file->private_data;

	printk("=> entered read function <=\n");																								//For debugging purpose

	if(!rb_device->current_traversal) {																											//if current traversal is null for the very first time, then we need to start reading from first node of tree
		rb_device->current_traversal = rb_first(&(rb_device->rootobj));
		if(!rb_device->current_traversal) {																										//if tree is still empty then return -1
			return -1;
		}

		rb_read_data  = rb_entry(rb_device->current_traversal, rb_traverse, rbnode);					//go to the first node's container structure
		read_buff = kmalloc(sizeof(struct rb_user_buff), GFP_KERNEL);													//allocating space to our buffer strcutre
		read_buff->key = rb_read_data->key;																										//copy the key and data
		read_buff->data = rb_read_data->data;
		printk("1=> Key : [%d] Data: [%d]\n", read_buff->key, read_buff->data);								//For debugging purpose

		copy_to_user(buf, read_buff, count);																									//Send the data to user

	} else if ((rb_device->current_traversal != NULL) && (rb_device->read_order == 0)) {    //Else condition is executed when read function has been called atleast once & order is ascending 

		if(!rb_next(rb_device->current_traversal)) {																					//If we've reached the end of the tree(leaf node)
			printk("No data to read\n");
			return -1;
		} else {
			rb_device->current_traversal = rb_next(rb_device->current_traversal);              //Go to the next node from last read node
		}

		if(rb_device->current_traversal != NULL) {																					//Check again if this node is null (leaf node)
			rb_read_data  = rb_entry(rb_device->current_traversal, rb_traverse, rbnode);			//Get the data
			read_buff = kmalloc(sizeof(struct rb_user_buff), GFP_KERNEL);
			read_buff->key = rb_read_data->key;
			read_buff->data = rb_read_data->data;
			printk("2=> Key : [%d] Data: [%d]\n", read_buff->key, read_buff->data);

			copy_to_user(buf, read_buff, count);																							//Send the data to user

			kfree(read_buff);
			rb_read_data = NULL;
			read_buff = NULL;
		}
	} else if ((rb_device->current_traversal != NULL) && (rb_device->read_order == 1)) {	//Else condition is executed when read function has been called atleast once & order is descending 
		if(!rb_prev(rb_device->current_traversal)) {																				//If we've reached the end of the tree(leaf node)				
			printk("No data to read\n");
			return -1;
		} else {
			rb_device->current_traversal = rb_prev(rb_device->current_traversal);            //Go to previous node (order is descending) 
		}

		if(rb_device->current_traversal != NULL) {																					//Check again if this node is null (leaf node)																				
			rb_read_data  = rb_entry(rb_device->current_traversal, rb_traverse, rbnode);
			read_buff = kmalloc(sizeof(struct rb_user_buff), GFP_KERNEL);
			read_buff->key = rb_read_data->key;
			read_buff->data = rb_read_data->data;
			printk("3=> Key : [%d] Data: [%d]\n", read_buff->key, read_buff->data);

			copy_to_user(buf, read_buff, count);																							//Send the data to user

			kfree(read_buff);																																	//Free all the memory created during this function
			rb_read_data = NULL;
			read_buff = NULL;
		}
	}
	return 0;
}

long int rb_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rb_dev *rb_dev_obj;																									
	rb_dev_obj = file->private_data;																						//Assigning the address of our character device structure
	rb_dev_obj->read_order = arg;																								//Update the read order as provided by the user for future references of read call
	printk("read_order = %d\n", rb_dev_obj->read_order);													//Debugging purpose
	return 0;
}

static struct file_operations rb_fops = {    // Fill in the file operation structure with desired file operations
	.owner            =   THIS_MODULE,  	     // and to in	initialize the character device structure with that.
	.open             =   rb_driver_open,
	.release          =   rb_driver_release,
	.write            =   rb_driver_write,
	.read             =   rb_driver_read,
	.unlocked_ioctl   =   rb_driver_ioctl,
};

int __init rb_driver_init(void)							//Initialization function of our driver
{ 
	int re_turn;															//To check the status of various function calls
	int time_since_boot; 
	
	printk("******** rb_tree init() ********\n");

	if(alloc_chrdev_region(&rb_dev_number, 0, 2, DEVICE_NAME) < 0 )    // Dynamic allocation of 32-bit number(Major,Minor)     
	{
		printk(KERN_DEBUG "Couldn't register the device\n"); 
		return -1;
	}

	//Populate the sysfs entry
	rb_dev_class = class_create(THIS_MODULE,DEVICE_NAME);
	
	//r-w permission for using the module inside my laptop
	//rb_dev_class->devnode = set_devnode; 
	
	//Memory allocation inside kernel space for our rb_dev structure
	rb_device  = kmalloc(sizeof(struct rb_dev), GFP_KERNEL);
	rb_device2 = kmalloc(sizeof(struct rb_dev), GFP_KERNEL);

	if(!rb_device)
	{
		printk("Couldn't allocate memory \n"); 
		return -ENOMEM;
	}

	if(!rb_device2)
	{
		printk("Couldn't allocate memory \n"); 
		return -ENOMEM;
	}
	
	//Initialising the character device structure with file operation structure
	cdev_init(&(rb_device->cdev),  &rb_fops);
	cdev_init(&(rb_device2->cdev), &rb_fops);
	
	rb_device->cdev.owner  = THIS_MODULE;
	rb_device2->cdev.owner = THIS_MODULE;
	
	//Hand this strucutre to Virtual File System
	re_turn = cdev_add(&(rb_device->cdev),MKDEV(MAJOR(rb_dev_number), 0),1);

	if(re_turn) 
	{
		printk("Couldn't add to cdev file (1) \n");
		return re_turn;
	}

	re_turn = cdev_add(&(rb_device2->cdev),MKDEV(MAJOR(rb_dev_number),1),1);

	if(re_turn)
	{
		printk("Couldn't add to cdev file (2) \n");
	}
	//Creates a device and register it with sysfs 
	rb_dev_device   = device_create(rb_dev_class, NULL, MKDEV(MAJOR(rb_dev_number),0),NULL,DEVICE_NAME_1);
	rb_dev_device2  = device_create(rb_dev_class, NULL, MKDEV(MAJOR(rb_dev_number),1),NULL,DEVICE_NAME_2);
	
	time_since_boot=(jiffies-INITIAL_JIFFIES)/HZ;
	return 0;
}	

void __exit rb_driver_exit(void)
{	
	// Release the major and minor number 
	unregister_chrdev_region((rb_dev_number),1);
	
	// Remove the device from sysfs
	device_destroy(rb_dev_class, MKDEV(MAJOR(rb_dev_number),0));
	device_destroy(rb_dev_class, MKDEV(MAJOR(rb_dev_number),1));
	// Delete the device
	cdev_del(&(rb_device->cdev));
	cdev_del(&(rb_device2->cdev));
	// Release the memory associated with rb_dev structure from kernel space
	kfree(rb_device);
	kfree(rb_device2);
	// Delete the driver class
	class_destroy(rb_dev_class);

	printk("******** rb_tree exit() ********\n");
}

module_init(rb_driver_init);
module_exit(rb_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHYAM");
MODULE_DESCRIPTION("RB-tree driver");
