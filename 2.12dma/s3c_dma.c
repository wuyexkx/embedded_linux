#include <linux/init.h>     // module_init module_exit
#include <linux/module.h>   // MODULE_LICENSE
#include <linux/fs.h>       // file_operations
#include <linux/cdev.h>     // cdev
#include <linux/kernel.h>
#include <linux/device.h>   // class_device
#include <asm/uaccess.h>    // copy_from_user copy_to_user
#include <asm/io.h>         // ioremap  iounmap
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

#define MEM_CPY_NO_DMA  0
#define MEM_CPY_DMA     1
#define BUF_SIZE        (512 * 1024) // 512KByte
#define DMA_BASE_ADDR0  (0x4B000000)
#define DMA_BASE_ADDR1  (0x4B000040)
#define DMA_BASE_ADDR2  (0x4B000080)
#define DMA_BASE_ADDR3  (0x4B0000C0)

static char* src;
static u32 src_phys;
static char* dst;
static u32 dst_phys;

// 1 确定主设备号
static int major;
static struct class* cls;

struct dma_regs {
    unsigned long disrc;
    unsigned long disrcc; 
    unsigned long didst; 
    unsigned long didstc; 
    unsigned long dcon; 
    unsigned long dstat; 
    unsigned long dcsrc; 
    unsigned long dcdst; 
    unsigned long dmasktrig; 
};
static volatile struct dma_regs *dmaregs;
// 定义休眠等待才队列
static DECLARE_WAIT_QUEUE_HEAD(dma_waitq);
// 标志，在中断中置1， 在ioctl中清0
static int ev_dma = 0;

static int s3c_dma_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

// 2 构造file_operations 
static struct file_operations dma_fops = {
    .owner = THIS_MODULE,
    .ioctl = s3c_dma_ioctl,
};
static int s3c_dma_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int i;
    memset(src, 0xAA, BUF_SIZE);
    memset(dst, 0x55, BUF_SIZE);

    switch (cmd)
    {
        case MEM_CPY_NO_DMA:
        {
            for (i=0; i<BUF_SIZE; ++i)
                dst[i] = src[i];
            if (memcmp(dst, src, BUF_SIZE) == 0)  // 比较以下数据是否都相等
                printk("MEM_CPY_NO_DMA ok.\n");
            else
                printk("MEM_CPY_NO_DMA error.\n"); 
            break;
        }
        case MEM_CPY_DMA:
        {
            ev_dma = 0;
            dmaregs->disrc  = src_phys;                 // 源物理地址
            dmaregs->disrcc = (0<<1) | (0<<0);          // 源位于AHB总线，源地址递增
            dmaregs->didst  = dst_phys;                 // 目的物理地址
            dmaregs->didstc = (0<<2) | (0<<1) | (0<<0); // 目的位于AHB总线，目的地址递增
            dmaregs->dcon   = (1<<30) | (1<<29) | (0<<28) | (1<<27) | (0<<23) | (0<<20) | (BUF_SIZE<<0);// 使能中断，单个传输，软件触发
            // 启动DMA
            dmaregs->dmasktrig = (1<<1) | (1<<0);

            // 休眠，传输完成后在irq函数里被唤醒，如果ev_dma为0则休眠
            wait_event_interruptible(dma_waitq, ev_dma);
            
            if (memcmp(dst, src, BUF_SIZE) == 0) // 比较以下数据是否都相等
                printk("MEM_CPY_DMA ok.\n");
            else
                printk("MEM_CPY_DMA error.\n");       

            break;
        }
    }
    return 0;
}

static irqreturn_t dma_irq(int irq, void *devid)
{
    // 唤醒
    ev_dma = 1;
    wake_up_interruptible(&dma_waitq);
    return IRQ_HANDLED;
}

static int s3c_dma_init(void)
{
    // 注册中断，DMA传输完成产生中断
    //  0～2已经被使用了，用3
    if (request_irq(IRQ_DMA3, dma_irq, 0, "s3c_dma", 1))
    {
        printk("can't request_irq for DMA3.\n");
        return -EBUSY;
    }
    // 申请较大连续空间，不能用kmalloc和vmalloc
    src = dma_alloc_writecombine(NULL, BUF_SIZE, &src_phys, GFP_KERNEL);
    if (NULL == src)
    {
        printk("can't alloc src buffer for 'src'.\n");
        return -ENOMEM;
    }
    dst = dma_alloc_writecombine(NULL, BUF_SIZE, &dst_phys, GFP_KERNEL);
    if (NULL == dst)
    {
        dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
        printk("can't alloc src buffer for 'dst'.\n");
        return -ENOMEM;
    }  

    major = register_chrdev(0, "s3c_dma", &dma_fops);
    cls = class_create(THIS_MODULE, "s3c_dma");
    class_device_create(cls, NULL, MKDEV(major, 0), NULL, "s3c_dma");

    dmaregs = ioremap(DMA_BASE_ADDR3, sizeof(struct dma_regs));

    return 0;
}

static void s3c_dma_exit(void)
{
    iounmap(dmaregs);
    class_device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, "s3c_dma");
    dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
    dma_free_writecombine(NULL, BUF_SIZE, dst, dst_phys);
    free_irq(IRQ_DMA3, 1);

}

module_init(s3c_dma_init);
module_exit(s3c_dma_exit);
MODULE_LICENSE("GPL");
