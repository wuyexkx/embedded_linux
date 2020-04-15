#include <linux/module.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

// 分配  设置  注册一个platform_driver

#define DEVICE_NAME  "myled"
static int major;

static struct class *led_class;
static struct class_device *led_class_dev;
static volatile unsigned long *gpio_con;
static volatile unsigned long *gpio_dat;
static int pin;

//  配置GPF4/5/6为输出模式
static int led_open(struct inode *inode, struct file *filep)
{
    // 配置为输出引脚
    *gpio_con &= ~(0x3<<(pin*2)); // 先清零 8-13 01010101
    *gpio_dat |=   (0x1<<(pin*2); // 再对应位置1
    // printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
//  配置GPF4/5/6输出高或者低电平
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    // 在用户层传入的形式是 write(fd, &val, 4); val是buf，4是count
    // 在内核层获取用户层传入的参数，用copy_from_user； 内核空间到用户传参数copy_to_user
    int user_param = 0;
    copy_from_user(&user_param, buf, count); // 从buf拷贝到user_param，长度为count
    if (1 == user_param) {       // 开灯
        // 4-6 000
        *gpio_dat &= ~(0x1<<pin);     
    }
    else if (0 == user_param) {  // 关灯
        *gpio_dat |= (0x1<<pin);
    }
    // printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
static struct file_operations fops = {
    .owner  = THIS_MODULE,
    .open   = led_open,
    .write  = led_write,
};

// platform_device_register注册后device_add加入到dev，
//  找能否支持看name相同否，找到则调用probe （dev中与drv中）
static int led_probe(struct platform_device *pdev)
{
    printk("led_probe found led.\n");
    // 根据platform_device的平台资源 获得寄存器和引脚资源，进行ioremap 
    //  参考drivers\pcmcia\at91_cf.c的at91_cf_probe
	struct resource	*resc;
	resc = platform_get_resource(pdev, IORESOURCE_MEM, 0); // 获取寄存器资源
	if (!resc)
		return -ENODEV;
    // led寄存器的地址映射，只需要执行一次，所以在入口函数映射, 起始 映射长度
    gpio_con = ioremap(resc->start, resc->end - resc->start + 1);
    gpio_dat = gpio_con + 1; 
    resc = platform_get_resource(pdev, IORESOURCE_IRQ, 0); // 获取引脚资源，这类资源的第0个
    pin = resc->start; // 根据资源修改pin值

    // 注册字符设备驱动程序
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(DEVICE_NAME "can't register major number.\n");
        return major;
    }
    led_class = class_create(THIS_MODULE, "led_class");
    if (IS_ERR(led_class))
        return PTR_ERR(led_class);
    led_class_dev = class_device_create(led_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);  // /dev/DEVICE_NAME
    if (unlikely(IS_ERR(led_class_dev)))
        return PTR_ERR(led_class_dev);


    return 0;
}
// platform_device_unregister卸载时，从dev中去掉，调用remove  （dev中与drv中）
static int led_remove(struct platform_device *pdev)
{
    printk("led_remove remove led.\n");

    // 卸载字符设备驱动程序
    class_device_destroy(led_class, MKDEV(major, 0)); // call to class_device_create()
    class_destroy(led_class_dev);
    unregister_chrdev(major, DEVICE_NAME);
    iounmap(gpio_con);

    return 0;
}

static struct platform_driver led_drv = {
	.driver.name	= DEVICE_NAME, // name dev和drv要一样
	.probe		= led_probe,
	.remove		= led_remove,
};

static int led_drv_init(void)
{
    platform_driver_register(&led_drv);
    return 0;
}
static void led_drv_exit(void)
{
    platform_driver_unregister(&led_drv);
}
module_init(led_drv_init);
module_exit(led_drv_exit);
MODULE_LICENSE("GPL");