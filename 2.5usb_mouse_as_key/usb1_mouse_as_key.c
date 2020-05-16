#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct usb_device_id usb_mouse_key_id_table [] = {
    // 接口描述符的成员，符合这三条就支持，HID类，子类是BOOT，协议是MOUSE
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT, 
		USB_INTERFACE_PROTOCOL_MOUSE) },
    // { USB_DEVICE(0x111,0x222) }, // 只支持某个厂家生产的某款设备，
	{ }	/* Terminating entry */
};

static int usb_mouse_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    printk("find usb mouse.\n");
    return 0;
}

static void usb_mouse_key_disconnect(struct usb_interface *intf)
{
    printk("disconnect usbmouse.\n");
}

// 1.分配 设置usb_driver
static struct usb_driver usbmouse_key_driver = {
    .name = usbmouse_key_name,
    .probe = usbmouse_key_probe,
    .disconnect = usbmouse_key_disconnect,
    .id_table = usbmouse_key_id_table,
};

static int usb_mouse_key_init(void)
{
    usb_register(&usbmouse_key_driver);
    return 0;
}

static void usb_mouse_key_exit(void)
{
    usb_deregister(&usbmouse_key_driver);
}
module_init(usb_mouse_key_init);
module_exit(usb_mouse_key_exit);
MODULE_LiCENSE("GPL");
