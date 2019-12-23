#include <linux/init.h>     // module_init module_exit
#include <linux/module.h>   // MODULE_LICENSE
#include <linux/fs.h>       // file_operations
#include <linux/cdev.h>     // cdev
#include <linux/kernel.h>
#include <linux/device.h>   // class_device
#include <linux/irq.h>      // IRQ_EINTx
#include <asm/uaccess.h>    // copy_from_user copy_to_user
#include <asm/io.h>         // ioremap  iounmap
#include <asm/irq.h>


#define DEVICE_NAME      "keys_int"
int major;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;
#define GPFCON      (*gpfcon)
#define GPFDAT      (*gpfdat)
#define GPGCON      (*gpgcon)
#define GPGDAT      (*gpgdat)

// 用于自动创建设备节点的结构class 和 class_device
static struct class *keys_class;
static struct class_device *keys_class_dev;


// 发生中断之后会调用这个函数, irq就是对应的中断号
static irqreturn_t keys_irq(int irq, void *dev_id)
{
    printk("irq = %d\n", irq);
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
// GPF0/2 GPG3/11 配置成输入模式
static int keys_open (struct inode *inode, struct file *filep)
{
    // GPFCON &= ~((0x3<<0*2) | (0x3<<2*2));  // 清零
    // GPGCON &= ~((0x3<<3*2) | (0x3<<11*2)); // 清零

    /*
    *	@irq: Interrupt line to allocate
    *	@handler: Function to be called when the IRQ occurs
    *	@irqflags: Interrupt type flags
    *	@devname: An ascii name for the claiming device
    *	@dev_id: A cookie passed back to the handler function
    */
    // 中断引脚，自动设备为中断引脚，
    //  IRQ_EINT0在include/asm-arm/arch-s3c2410/irqs.h中定义
    //  keys_irq
    //  IRQT_BOTHEDGE双边沿触发
    //  "EINT0_S2"
    request_irq(IRQ_EINT0,  keys_irq, IRQT_BOTHEDGE, "EINT0_S2", 1);
    request_irq(IRQ_EINT2,  keys_irq, IRQT_BOTHEDGE, "EINT0_S3", 1);
    request_irq(IRQ_EINT11, keys_irq, IRQT_BOTHEDGE, "EINT0_S4", 1);
    request_irq(IRQ_EINT19, keys_irq, IRQT_BOTHEDGE, "EINT0_S5", 1);

    return 0;
}
ssize_t keys_read (struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned char key_vals[4];
    int regval;

    // 判断缓冲区大小
    if (size != sizeof(key_vals))
        return -EINVAL;

    // 获取GPFDAT寄存器的值
    regval = GPFDAT;
    key_vals[1] = (regval & (0x1 << 0)) ? 1 : 0; 
    key_vals[2] = (regval & (0x1 << 2)) ? 1 : 0; 
    // 获取GPGDAT寄存器的值
    regval = GPGDAT;
    key_vals[3] = (regval & (0x1 << 3)) ? 1 : 0; 
    key_vals[0] = (regval & (0x1 << 11)) ? 1 : 0; 

    // 将键值返回到用户空间
    copy_to_user(buf, key_vals, size);

    return sizeof(key_vals);
}
// 释放中断的配置
int key_close(struct inode *inode, struct file *filp)
{
    free_irq(IRQ_EINT0, 1);
    free_irq(IRQ_EINT2, 1);
    free_irq(IRQ_EINT11,1);
    free_irq(IRQ_EINT19,1);
    return 0;
}
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = keys_open,
    .read    = keys_read,
    .release = key_close,
};

static int keys_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0)
    {
        printk(DEVICE_NAME " can't register major number.\n");
        return major;
    }

    keys_class = class_create(THIS_MODULE, "keys_class");
    if (IS_ERR(keys_class))
        return PTR_ERR(keys_class);
    // 2. 在class里边创建一个设备叫xxx，然后mdev自动创建设备节点/dev/xxx
    //  在/dev目录下创建相应的设备节点，
    keys_class_dev = class_device_create(keys_class, NULL, MKDEV(major, 0), NULL, "keys_intnode"); 
    if (unlikely(IS_ERR(keys_class_dev)))
        return PTR_ERR(keys_class_dev); 

    gpfcon = (volatile unsigned long*)ioremap(0x56000050, 16);
    gpfdat = gpfcon + 1;
    gpgcon = (volatile unsigned long*)ioremap(0x56000060, 16);
    gpgdat = gpgcon + 1;

    printk(DEVICE_NAME " device initialized successfully...\n\n");

    return 0;
}

static void keys_exit(void)
{
    // 对应卸载
    unregister_chrdev(major, DEVICE_NAME);

    class_device_unregister(keys_class_dev); 
    class_destroy(keys_class);

    iounmap(gpfcon);
    iounmap(gpgcon);
}

module_init(keys_init);
module_exit(keys_exit);
MODULE_LICENSE("GPL");
