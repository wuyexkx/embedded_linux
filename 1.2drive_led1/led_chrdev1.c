#include <linux/init.h>     // module_init module_exit
#include <linux/module.h>   // MODULE_LICENSE
#include <linux/fs.h>       // file_operations
#include <linux/cdev.h>     // cdev
#include <linux/kernel.h>
#include <linux/device.h>   // class_device
#include <asm/uaccess.h>    // copy_from_user copy_to_user
#include <asm/io.h>         // ioremap  iounmap

// GPF引脚的con和dat寄存器，地址需要映射
volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;

// 用于自动创建设备节点的结构class 和 class_device
static struct class *demo_class;
static struct class_device *demo_class_devs;

// 操作方法
//  配置GPF4/5/6为输出模式
static int led_open(struct inode *inode, struct file *filep)
{
    *gpfcon &= ~((0x3<<(4*2)) | (0x3<<(5*2)) | (0x3<<(6*2)));   // 先清零 8-13 01010101
    *gpfcon |=   (0x1<<(4*2)  |  0x1<<(5*2)  |  0x1<<(6*2));    // 再对应位置1
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
//  配置GPF4/5/6输出高或者低电平
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    // 在用户层传入的形式是 write(fd, &val, 4); val是buf，4是count
    // 在内核层获取用户层传入的参数，用copy_from_user； 内核空间到用户传参数copy_to_user
    int user_param = 0;
    copy_from_user(&user_param, buf, count); // 从buf拷贝到user_param，长度为count
    if (1 == user_param)        // 开灯
    {
        // 4-6 000
        *gpfdat &= ~((0x7<<4));     
    }
    else if (0 == user_param)   // 关灯
    {
        *gpfdat |= (0x7<<4);
    }
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);

    return 0;
}
// 操作方法集
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open  = led_open,
    .write = led_write,
};

// 用于保存获取到的主设备号
unsigned int major = 0; 

// 入口函数
static int demo_cdev_init(void)
{
    // 自动获取主设备号资源；主设备号对应的名字在/proc/devices中，
    //  内核驱动模块的名字是Makefile里指定的（一般跟驱动.c文件名字对应）
    major = register_chrdev(0, "major_name_led", &fops); //  0自动分配， 
    
    // 以下为 在系统中生成设备信息的步骤
    // 1. 新建一个class
    demo_class = class_create(THIS_MODULE, "led_chrdev_class1");
    if (IS_ERR(demo_class))
        return PTR_ERR(demo_class);
    // 2. 在class里边创建一个设备叫xxx，然后mdev自动创建设备节点/dev/xxx
    //  在/dev目录下创建相应的设备节点
    demo_class_devs = class_device_create(demo_class, NULL, MKDEV(major, 0), NULL, "led_on_off1"); 
    if (unlikely(IS_ERR(demo_class_devs)))
        return PTR_ERR(demo_class_devs);

    // led寄存器的地址映射，只需要执行一次，所以在入口函数映射
    gpfcon = (volatile unsigned long*)ioremap(0x56000050, 16); // start, size
    gpfdat = gpfcon + 1;                                       // unsigned long的长度

    return 0;
}
// 出口函数
static void demo_cdev_exit(void)
{
    // 对应卸载
    unregister_chrdev(major, "major_name_led");
    class_device_unregister(demo_class_devs);  
    class_destroy(demo_class);

    // 去掉映射关系
    iounmap(gpfcon);
}

// 内核模块相关
module_init(demo_cdev_init);
module_exit(demo_cdev_exit);
MODULE_LICENSE("GPL");