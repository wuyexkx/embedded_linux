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
#include <asm/arch/regs-gpio.h> // S3C2410_GPFx
#include <linux/poll.h>
#include <linux/timer.h>   // HZ
#include <linux/jiffies.h> // jiffies

#define DEVICE_NAME      "keys_all"
int major;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;
#define GPFCON      (*gpfcon)
#define GPFDAT      (*gpfdat)
#define GPGCON      (*gpgcon)
#define GPGDAT      (*gpgdat)

// 用定时器来防抖动, 主要是设置超时时间,处理函数
// 定义一个定时器
static struct timer_list keys_timer;
static struct pin_desc *irq_pd;

// 使用static来允许同一时刻只有一个用户使用驱动程序
// static int canopen = 1;
// 使用原子操作,定义原子变量并初始化为1
// static atomic_t canopen = ATOMIC_INIT(1);
// 使用信号量， 
static DECLARE_MUTEX(canopen); // 定义互斥锁

// 异步通知用户程序 需要的保存用户pid等信息的结构体
static struct fasync_struct *keys_async_stct;

// 用于自动创建设备节点的结构class 和 class_device
static struct class *keys_class;
static struct class_device *keys_class_dev;

/* 定义一个等待队列button_waitq，这个等待队列实际上是由中断驱动的，
    当中断发生时，会令挂接到这个等待队列的休眠进程唤醒 */
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
// 中断事件标志，ISR将它置1, read 清0
static volatile int ev_press = 0;
// 自定义引脚描述的结构
struct pin_desc
{
    unsigned int pin;
    unsigned int key_val;
};
/*键值：按下，0x01,0x02,0x03,0x04
  键值：松开，0x81,0x82,0x83,0x84*/
static unsigned char key_val;
struct pin_desc pins_desc[4] = {
    {S3C2410_GPF0,  0x01},
    {S3C2410_GPF2,  0x02},
    {S3C2410_GPG3,  0x03},
    {S3C2410_GPG11, 0x04}
};
// ISR 发生中断之后会调用这个函数, irq就是对应的中断号
static irqreturn_t keys_irq(int irq, void *dev_id)
{
    irq_pd = (struct pin_desc *)dev_id;
    // 修改超时时间jiffies, jiffies是全局变量,每隔10ms系统产生一个时钟中断, 10ms
    //  每次进中断都修改jiffies往后延迟10ms, 10ms内可能多次进入中断,
    //  直到抖动到最后一次进中断,然后调用keys_timer_function去获取按键的状态
    mod_timer(&keys_timer, jiffies+HZ/100);

    return IRQ_RETVAL(IRQ_HANDLED);
}

// GPF0/2 GPG3/11 申请中断
static int keys_open (struct inode *inode, struct file *filp)
{   
    // // static同时只有一个用户使用drv
    // if (--canopen != 0)
    // {
    //     // 这个++是配合判断里的--
    //     canopen++;
    //     return -EBUSY;
    // }
    // 原子变量自减test, 如果自减之后为0则返回true
    // if (!atomic_dec_and_test(&canopen))
    // {
    //     // 这个inc是配合判断里的dec
    //     atomic_inc(&canopen);
    //     return -EBUSY;
    // }
    // 获取信号量，获取不到不可被打断，kill不掉
    // down(&canopen); // down_interruptible();获取不到就休眠，休眠中可以被打断

    // 阻塞与非阻塞
    if (filp->f_flags & O_NONBLOCK) // 非阻塞，
    {
        // 非阻塞，如果获取不了锁就应该立即返回
        if (down_trylock(&canopen))
            return -EBUSY;
    }
    else                            // 阻塞
    {
        // 获取信号量，获取不到就休眠等待那个锁，kill不掉
        down(&canopen); // down_interruptible();获取不到就休眠，休眠中可以被打断
    }
    
    /**********************************************************
    中断控制器的初始化（底层）在irq_chip结构体中定义；
    自定义的处理函数以desc为索引，在action链表中；
        struct irq_desc
        struct irq_chip
        action
    需用通过注册的机制将他们联系起来:
        request_irq()
        free_irq()
    中断的顶半部和底半部：
        顶半部：
            在request_irq中传入的handler就是它，会屏蔽中断，行紧急之事开销小，如flag，读写IO寄存器等等，然后将下半部处理函数挂在到底半部的执行队列中去
        底半部：
            执行耗时工作，可以被打断，底半部机制包括： 
                tasklet：使系统在适当时调度运行
                工作队列：将工作推后执行的机制，推后的工作交由一个内核线程去执行，优势是允许重新调度甚至睡眠
                软中断：一般写驱动不用也宜使用
    ***********************************************************/
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
    // IRQ_EINT0 &pins_desc[0]将被传入到中断服务程序
    request_irq(IRQ_EINT0,  keys_irq, IRQT_BOTHEDGE, "EINT0_S2", &pins_desc[0]);
    request_irq(IRQ_EINT2,  keys_irq, IRQT_BOTHEDGE, "EINT0_S3", &pins_desc[1]);
    request_irq(IRQ_EINT11, keys_irq, IRQT_BOTHEDGE, "EINT0_S4", &pins_desc[2]);
    request_irq(IRQ_EINT19, keys_irq, IRQT_BOTHEDGE, "EINT0_S5", &pins_desc[3]);

    return 0;
}
// read函数会被用户空间调用，可能while(1), 防止占用资源
ssize_t keys_read (struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    // 判断缓冲区为1
    if (size != 1)
        return -EINVAL;

    if (filp->f_flags & O_NONBLOCK) // 非阻塞
    {
        if (!ev_press) // 没有按键中断产生
            return -EAGAIN;
    }
    else                            // 阻塞
    {
        // 没有按键动作就休眠, 内部判断ev_press是否为0，为0则休眠，否则往下执行
        //  有休眠就有唤醒，唤醒在ISR中
        wait_event_interruptible(button_waitq, ev_press);
    }

    // // 没有按键动作就休眠, 内部判断ev_press是否为0，为0则休眠，否则往下执行
    // //  有休眠就有唤醒，唤醒在ISR中
    // wait_event_interruptible(button_waitq, ev_press);

    // 将键值返回到用户空间
    copy_to_user(buf, &key_val, 1);
    // 中断事件标志，ISR将它置1, read 清0
    ev_press = 0;

    return 1;
}
// 释放中断的配置
int keys_close(struct inode *inode, struct file *filp)
{
    // canopen++; // 使用完配合前面打开时的--
    // atomic_inc(&canopen); // 这个inc是配合判断里的dec
    up(&canopen); // 释放掉信号量

    free_irq(IRQ_EINT0, &pins_desc[0]);
    free_irq(IRQ_EINT2, &pins_desc[1]);
    free_irq(IRQ_EINT11,&pins_desc[2]);
    free_irq(IRQ_EINT19,&pins_desc[3]);
    return 0;
}
static unsigned keys_poll (struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    // 将进程挂接到button_waitq等待队列下, 前面定义的button_waitq
    poll_wait(filp, &button_waitq, wait);
    // 根据实际情况，标记事件类型 
    if (ev_press)
        mask |= POLLIN | POLLRDNORM;
    // 如果mask为0，则没有请求事件发生；如果非零说明有事件发生
    return mask;
}
static int keys_drv_async (int fd, struct file *filp, int on)
{
    // 用helper去初始化fasync_struct这个结构体，初始化之后就可以在IRQ中使用kill_fasync
    return fasync_helper (fd, filp, on, &keys_async_stct);
}
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = keys_open,
    .read    = keys_read,
    .release = keys_close,
    .poll    = keys_poll,
    .fasync  = keys_drv_async,
};

// 时间处理函数
static void keys_timer_function(unsigned long data)
{
    // irq_pd是在中断函数中修改的引脚描述, 得到是哪个引脚引起的中断
    struct pin_desc *pindesc = irq_pd;
    unsigned int pin_status;
    
    if (!pindesc)
        return;
    
    // 获取该引脚状态
    pin_status = s3c2410_gpio_getpin(pindesc->pin);
    // printk("irq = %d\n", irq);
    // 判断引脚，给全局变量赋对应引脚的状态值 （状态--> 所属引脚）
    if (pin_status) // 松开
    {
        key_val = 0x80 | pindesc->key_val; // 存入自定义全局变量，在read中to_user
    }
    else            // 按下
    {
        key_val = 0x00 | pindesc->key_val; // 存入自定义全局变量，在read中to_user
    }
    ev_press = 1; // 中断事件标志，ISR将它置1, read 清0
    wake_up_interruptible(&button_waitq);

    // 向用户进程发送信号, 发给谁，在fasync_struct结构中应该指出，在此之前keys_async_stct应该在helper中被初始化
    //  用户程序用fcntl(fd, F_SETOWN, pid)告诉驱动程序，用户调用它时系统调用fops.fasync初始化fasync_struct
	kill_fasync (&keys_async_stct, SIGIO, POLL_IN);

    return IRQ_RETVAL(IRQ_HANDLED);
}

static int keys_init(void)
{
    // 初始化
    init_timer(&keys_timer);
    // 处理函数
    keys_timer.function = keys_timer_function; 
    // keys_timer.expires = 0;
    add_timer(&keys_timer);


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
    keys_class_dev = class_device_create(keys_class, NULL, MKDEV(major, 0), NULL, "keys_all_node"); 
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
