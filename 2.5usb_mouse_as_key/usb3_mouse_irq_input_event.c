#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/usb.h>

static struct input_dev *usbk_dev;
static char *usb_buf;            // 虚拟地址
static dma_addr_t usb_buf_phys;  // 物理地址 typedef u64 dma_addr_t;
static int len;
struct urb *usbk_urb;

static struct usb_device_id usb_mouse_key_id_table [] = {
    // 接口描述符的成员，符合这三条就支持，HID类，子类是BOOT，协议是MOUSE
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT, 
		USB_INTERFACE_PROTOCOL_MOUSE) },
    // { USB_DEVICE(0x111,0x222) }, // 只支持某个厂家生产的某款设备，
	{ }	/* Terminating entry */
};

// USB控制器在接收到数据后会产生中断，cpu调用这个irq
static void usbmouse_key_irq(struct urb *urb)
{
    // 打印缓冲区内容
    // int i;
    // static int cnt = 0;
    // printk("data cnt: %d ", ++cnt);
    // for (i=0; i<len; ++i) {
    //     printk("%02x ", usb_buf[i]); // 打印缓冲区内容
    // }

    static unsigned int pre_val;
    // USB数据含义  
        // USB设备驱动程序知道数据含义，总线驱动识别设备、找到驱动、提供读写函数
        // data[0]: bit0-左键 1-按下 0-松开
        //          bit1-左键 1-按下 0-松开
        //          bit2-左键 1-按下 0-松开
        // 与上次的值比较 发生了变换则上报事件
    if ((pre_val & (1<<0)) != (usb_buf[0] & (1<<0))) { 
        // 左键变化 ,有值1按下 否则0松开
        input_event(usbk_dev, EV_KEY, KEY_L, (usb_buf[0] & (1<<0)) ? 1 : 0);
        input_sync(usbk_dev);
    }
    if ((pre_val & (1<<1)) != (usb_buf[0] & (1<<1))) { 
        // 右键变化 ,有值1按下 否则0松开
        input_event(usbk_dev, EV_KEY, KEY_S, (usb_buf[0] & (1<<1)) ? 1 : 0);
        input_sync(usbk_dev);
    }
    if ((pre_val & (1<<2)) != (usb_buf[0] & (1<<2))) { 
        // 中键变化 ,有值1按下 否则0松开
        input_event(usbk_dev, EV_KEY, KEY_ENTER, (usb_buf[0] & (1<<2)) ? 1 : 0);
        input_sync(usbk_dev);
    }        
    pre_val = usb_buf[0]; // 保存当前值

    // 重新提交urb
    usb_submit_urb(usbk_urb, GFP_KERNEL);
}

static int usb_mouse_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    // 从usb_interface获取usb_device
    struct usb_device *usb_dev = interface_to_usbdev(intf);     
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;    
	int pipe;

	interface = intf->cur_altsetting;
	endpoint = &interface->endpoint[0].desc;

    // printk("find usb mouse.\n");
    // // usb_device中有struct usb_device_descriptor descriptor;/* Descriptor */
    // printk("bsdUSB   : %x\n", usb_dev->descriptor->bcdUSB);   // 版本号
    // printk("idVendor : %x\n", usb_dev->descriptor->idVendor); // 提供商
    // printk("bcdDevice: %x\n", usb_dev->descriptor->bcdDevice);// 产品id

    // 不打印 也不判断是否为输入端点 中断端点等，直接认为是鼠标
    // 1.分配一个input_dev
    usbk_dev = input_allocate_device();
    // 2.设置
    // 2.1能产生哪类事件
    set_bit(EV_KEY, usbk_dev->evbit);
    set_bit(EV_REP, usbk_dev->evbit); // 重复类事件，长按
    // 2.2能产生这类事件的哪些事件
    set_bit(KEY_L, usbk_dev->keybit);
    set_bit(KEY_S, usbk_dev->keybit);
    set_bit(KEY_ENTER, usbk_dev->keybit);
    // 3.注册
    input_register_device(usbk_dev);
    // 4.硬件相关操作  在之前的led lcd等硬件操作就是配置寄存器
    //      在usb中，是通过总线去收发usb数据
    // 数据传输三要素
    // 一源：USB设备的某个端点
        // pipe源 端点类型 USB设备的编号和端点的编号 端点方向
        // ((PIPE_INTERRUPT << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN) 
        // __create_pipe中return (dev->devnum << 8) | (endpoint << 15);    
	pipe = usb_rcvintpipe(usb_dev, endpoint->bEndpointAddress); 
    // 三长度：
    len = endpoint->wMaxPacketSize; // 最大包大小
    // 二目地：
	usb_buf = usb_buffer_alloc(usb_dev, len, GFP_ATOMIC, &usb_buf_phys);
    
    // 使用三要素
    // 分配一个USB请求块 usb request block
	usbk_urb = usb_alloc_urb(0, GFP_KERNEL);
    // 设置urb,填充这个urb
        // USB控制器得到数据后 会产生中断 cpu调用complete函数usbmouse_key_irq
	usb_fill_int_urb(usbk_urb, usb_dev, pipe, usb_buf,
			 len,
			 usbmouse_key_irq, NULL, endpoint->bInterval); // 查询频率
        //  USB控制器会将这些数据写入到内存，并不聪明所以需要告知物理地址
	usbk_urb->transfer_dma = usb_buf_phys;
        // 设置某些标记
	usbk_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;    

    // 使用urb
    usb_submit_urb(usbk_urb, GFP_KERNEL);

    return 0;
}

static void usb_mouse_key_disconnect(struct usb_interface *intf)
{
    struct usb_device *usb_dev = interface_to_usbdev(intf);     
    // printk("disconnect usbmouse.\n");
    usb_kill_urb(usbk_urb);
    usb_free_urb(usbk_urb);
    usb_buffer_free(usb_dev, len, usb_buf, usb_buf_phys);
    input_unregister_device(usbk_dev);
    input_free_device(usbk_dev);
}

// 1.分配 设置usb_driver
static struct usb_driver usbmouse_key_driver = {
    .name = "usbms_key_name",
    .probe = usb_mouse_key_probe,
    .disconnect = usb_mouse_key_disconnect,
    .id_table = usb_mouse_key_id_table,
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
