#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>

static struct spi_device *dot_matrix;

static struct spi_board_info dot_matrix_info = 
{
	.modalias = "MAX7219",
	.bus_num = 1,
	.chip_select = 1,
	.mode = 0,
	.max_speed_hz = 500000,
};





static int initialize(void)
{	
	struct spi_master *master;
    printk("entered init device \n");

	master = spi_busnum_to_master(dot_matrix_info.bus_num);
	if(!master)
    {
        printk("error assigining bus number to master \n");
        return -ENODEV;
    }

	dot_matrix = spi_new_device(master, &dot_matrix_info);

	if(!dot_matrix)
	{
        printk("error adding new Spi device\n");
        return -ENODEV;
    }
	
    dot_matrix->bits_per_word = 8;

	return 0;
}

static void cleanup(void)
{
    printk("entered cleanup device \n");
	spi_unregister_device(dot_matrix);
}

module_init(initialize);
module_exit(cleanup);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHYAM");
MODULE_DESCRIPTION("spi_device");