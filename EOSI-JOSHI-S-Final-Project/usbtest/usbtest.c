#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

int main (int argc, char *argv)
{
	unsigned char strDesc[256];
	ssize_t	totalUSBDev = 0;
	ssize_t id = 0;
	libusb_device **usbDevList = NULL;
	libusb_device *usbDevPtr = NULL;
	libusb_device_handle *usbDevHandle = NULL;
	libusb_context *usbCtx = NULL;
	struct libusb_device_descriptor usbDescp;
	int ret = 0;
	
	ret = libusb_init(&usbCtx);
	if(ret < 0) {
		printf ("Error in USB initialisation%d",ret);
		return ret;
    }

	// Get the list of USB devices visible to the system.
	totalUSBDev = libusb_get_device_list (usbCtx, &usbDevList);
	
	// loop through all USB devices
	while (id < totalUSBDev) {
		// take the next pointer of the usbDevList
		usbDevPtr = usbDevList[id++];

		// open the current USB device
		ret = libusb_open(usbDevPtr, &usbDevHandle);
		if (ret != LIBUSB_SUCCESS) {
			break;
		}
 
      	// Get the device descriptor for this device.
		ret = libusb_get_device_descriptor (usbDevPtr, &usbDescp);
		if (ret != LIBUSB_SUCCESS) {
			break;
		}

		if(usbDescp.idVendor == 0x403) {
			printf ("Productid \t: %x\n", usbDescp.idProduct);
			printf ("Vendorid \t: %x\n", usbDescp.idVendor);
			
			if (usbDescp.iManufacturer > 0) {
				ret = libusb_get_string_descriptor_ascii(usbDevHandle, usbDescp.iManufacturer, strDesc, 256);
				if (ret < 0)
					break;
				printf("Manufacturer \t: %s\n",  strDesc);
			}
		
			if (usbDescp.iProduct > 0) {
				ret = libusb_get_string_descriptor_ascii(usbDevHandle, usbDescp.iProduct, strDesc, 256);
				if (ret < 0)
					break;
				printf ("Product \t: %s\n", strDesc);
			}
			
			if (usbDescp.iSerialNumber > 0) {
				ret = libusb_get_string_descriptor_ascii(usbDevHandle, usbDescp.iSerialNumber, strDesc, 256);
				if (ret < 0)
					break;
				printf ("SerialNumber \t: %s\n", strDesc);
			}
		}
		libusb_close (usbDevHandle);
		usbDevHandle = NULL;
	}
	
	if (usbDevHandle != NULL) {
		libusb_close (usbDevHandle);
	}
	libusb_exit (usbCtx);
	return 0;
}