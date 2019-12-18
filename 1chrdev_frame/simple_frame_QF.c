#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>  // 操作方法集
#include <linux/cdev.h> 

#define BASEMINOR   0
#define COUNT       3
#define NAME        "chrdev_demo"

// 保存申请到的设备号
dev_t devno; 
// 抽象的字符设备结构体
struct cdev *cdevp;

// 操作方法集内的函数指针（函数）定义
int demo_open(struct inode *inode, struct file *filep)
{
    printk("---%s---%s---%d", __FILE__, __func__, __LINE__);
    return 0;
}
int demo_release(struct inode *inode, struct file *filep)
{
    printk("---%s---%s---%d", __FILE__, __func__, __LINE__);
    return 0;
}
// 定义操作方法集结构体，并部分初始化
struct file_operations fops = { 
    .owner   = THIS_MODULE,
    .open    = demo_open,
    .release = demo_release,
};

static int __init module_demo_init(void)
{
    int ret = 0; // 用于保存分配到的设备号

    // 0. 获取设备号 alloc_chrdev_region 自动获取设备号
    ret = alloc_chrdev_region(&devno, BASEMINOR, COUNT, NAME);
    if(ret < 0)
    {
        printk(KERN_ERR "alloc_chrdev_region failed.\n");
        goto err1_alloc_chrdev_region;
    }
    printk(KERN_INFO "major = %d\n", MAJOR(devno)); // 主设备号是内核分配的，次设备是自己指定，提取主设备号打印
    // 1. 分配内存 cdev_alloc
    cdevp = cdev_alloc();
    if(cdevp == NULL)
    {
        printk(KERN_ERR "cdev_alloc failed.\n");
        ret = -ENOMEM;     // #define	ENOMEM		12	/* Out of memory */
        goto err2_cdev_alloc;
    }
    // 2. 初始化cdev结构体 cdev_init
    cdev_init(cdevp, &fops);
    // 3. 注册设备到内核 cdev_add
    ret = cdev_add(cdevp, devno, COUNT);
    if(ret < 0)
    {
        printk(KERN_ERR "cdev_add failed.\n");
        goto err2_cdev_alloc;
    }
    printk("---%s---%s---%d", __FILE__, __func__, __LINE__); // 打印此行说明前面成功
    return 0;                                                // 成功之后就不能执行下面的err释放资源步骤

    // 释放资源，申请资源的倒序
    err2_cdev_alloc:
        unregister_chrdev_region(devno, COUNT);
    err1_alloc_chrdev_region:
        return ret;
}

static void __exit module_demo_exit(void)
{
    // 注销设备号
    unregister_chrdev_region(devno, COUNT); 
    // 注销字符设备
    cdev_del(cdevp);
    printk("---%s---%s---%d", __FILE__, __func__, __LINE__); // 打印此行说明注销成功
}

module_init(module_demo_init);
module_exit(module_demo_exit);

MODULE_LICENSE("GPL");