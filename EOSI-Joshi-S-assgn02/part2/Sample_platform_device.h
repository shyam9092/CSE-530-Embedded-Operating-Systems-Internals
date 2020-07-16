#include <linux/platform_device.h>

#ifndef __SAMPLE_PLATFORM_H__


  
#define __SAMPLE_PLATFORM_H__

struct P_chip 
{
	char 			*name;
	int			dev_no;
	struct platform_device 	plf_dev;
	struct device *HC_dev;
};



#endif /* __GPIO_FUNC_H__ */
