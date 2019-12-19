#include <linux/init.h>     // module_init module_exit
#include <linux/module.h>   // MODULE_LICENSE
#include <linux/fs.h>       // file_operations
#include <linux/cdev.h>     // cdev
#include <linux/kernel.h>
#include <linux/device.h>   // class_device
#include <asm/uaccess.h>    // copy_from_user copy_to_user
#include <asm/io.h>         // ioremap  iounmap

#define DEVICE_NAME     "leds" // 加载模式后，执行”cat /proc/devices”命令看到的设备名称
int major = 0;                 // 用于保存获取到的主设备号

// GPF引脚的con和dat寄存器，地址需要映射
volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;
#define GPFCON      (*gpfcon)
#define GPFDAT      (*gpfdat)

// 用于自动创建设备节点的结构class 和 class_device
static struct class *leds_class;
static struct class_device *leds_class_dev[4];

// 操作方法
//  根据次设备号 配置GPF4/5/6为输出模式
static int leds_open(struct inode *inode, struct file *filep)
{
    // 从inode中提取次设备号
	int minor = MINOR(inode->i_rdev); 
    
    switch(minor)
    {
        case 0: // leds
        {
            GPFCON &= ~((0x3<<(4*2)) | (0x3<<(5*2)) | (0x3<<(6*2)));   // 先清零 8-13 01010101
            GPFCON |=   (0x1<<(4*2)  |  0x1<<(5*2)  |  0x1<<(6*2));    // 再对应位置1
            break;
        }
        case 1: // led1
        {
            GPFCON &= ~(0x3<<(4*2));
            GPFCON |=   (0x1<<(4*2));
            break;
        }
        case 2: // led2
        {
            GPFCON &= ~(0x3<<(5*2));
            GPFCON |=   (0x1<<(5*2));
            break;
        }
        case 3: // led3
        {
            GPFCON &= ~(0x3<<(6*2));
            GPFCON |=   (0x1<<(6*2));
            break;
        }
    }
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);

    return 0;
}
//  配置GPF4/5/6输出高或者低电平
static ssize_t leds_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    // 获取次设备号
	int minor = MINOR(filp->f_dentry->d_inode->i_rdev);

    // 在用户层传入的形式是 write(fd, &val, 4); val是buf，4是count
    // 在内核层获取用户层传入的参数，用copy_from_user； 内核空间到用户传参数copy_to_user
    int user_param = 0;
    copy_from_user(&user_param, buf, count); // 从buf拷贝到user_param，长度为count
    
    switch(minor)
    {
        case 0:
        { 
            if (user_param == 1)
                GPFDAT |= (0x7 << 4);  
            else if (user_param == 0)
                GPFDAT &= ~(0x7 << 4);    // 4-6 000
            break;
        }
        case 1:
        { 
            if (user_param == 1)
                GPFDAT |= (0x1 << 4);  
            else if (user_param == 0)
                GPFDAT &= ~(0x1 << 4);    // 4 0
            break;
        }
        case 2:
        { 
            if (user_param == 1)
                GPFDAT |= (0x1 << 5);  
            else if (user_param == 0)
                GPFDAT &= ~(0x1 << 5);    // 5 0
            break;
        }
        case 3:
        { 
            if (user_param == 1)
                GPFDAT |= (0x1 << 6);  
            else if (user_param == 0)
                GPFDAT &= ~(0x1 << 6);    // 6 0
            break;
        }
    }
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);

    return 0;
}
// 操作方法集
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open  = leds_open,
    .write = leds_write,
};

// 入口函数
static int leds_init(void)
{
    int minor = 0; // 保存次设备号

    // led寄存器的地址映射，只需要执行一次，所以在入口函数映射
    gpfcon = (volatile unsigned long*)ioremap(0x56000050, 16); // start, size
    gpfdat = gpfcon + 1;                                       // unsigned long的长度

    // 0. 自动获取主设备号资源；主设备号对应的名字在/proc/devices中，
    //      内核驱动模块的名字是Makefile里指定的（一般跟驱动.c文件名字对应）
    major = register_chrdev(0, DEVICE_NAME, &fops); //  0自动分配， 
    if (major < 0)
    {
        printk(DEVICE_NAME " can't register major number.\n");
        return major;
    }

    // 以下为 在系统中生成设备信息的步骤
    // 1. 新建一个class
    leds_class = class_create(THIS_MODULE, "leds_class");
    if (IS_ERR(leds_class))
        return PTR_ERR(leds_class);
    // 2. 在class里边创建一个设备叫xxx，然后mdev自动创建设备节点/dev/xxx
    //  在/dev目录下创建相应的设备节点，0全部led设备节点
    leds_class_dev[0] = class_device_create(leds_class, NULL, MKDEV(major, 0), NULL, "led0"); 
    if (unlikely(IS_ERR(leds_class_dev[0])))
        return PTR_ERR(leds_class_dev[0]);
    //      1,2,3对应led1/2/3设备节点
    for (minor=1; minor<4; ++minor)
    {
        leds_class_dev[minor] = class_device_create(leds_class, NULL, MKDEV(major, minor), NULL, "led%d", minor); 
        if (unlikely(IS_ERR(leds_class_dev[minor])))
            return PTR_ERR(leds_class_dev[minor]);
    }    
    printk(DEVICE_NAME " device initialized successfully...\n\n");

    return 0;
}
// 出口函数
static void leds_exit(void)
{
    int minor = 0; // 保存次设备号

    // 对应卸载
    unregister_chrdev(major, DEVICE_NAME);
    for (minor=0; minor<4; ++minor)
    {
        class_device_unregister(leds_class_dev[minor]); 
    }    
    class_destroy(leds_class);
    // 去掉映射关系
    iounmap(gpfcon);
}

// 内核模块相关
module_init(leds_init);
module_exit(leds_exit);
MODULE_LICENSE("GPL");