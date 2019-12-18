#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h> 
#include <linux/cdev.h> 
#include <linux/kernel.h>
#include <linux/device.h> 

// 用于自动创建设备节点的结构
static struct class *demo_class;
static struct class_device *demo_class_devs;

// 操作方法
static int demo_cdev_open(struct inode *inode, struct file *filep)
{
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
static ssize_t demo_cdev_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    printk("--%s--%s--%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
// 操作方法集
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open  = demo_cdev_open,
    .write = demo_cdev_write,
};

// 用于保存获取到的主设备号
unsigned int major = 0; 

// 入口函数
static int demo_cdev_init(void)
{
    // 自动获取主设备号资源
    major = register_chrdev(0, "demo_chrdev_1", &fops); //  0自动分配
    
    // 以下为 在系统中生成设备信息的步骤
    // 1. 新建一个class
    demo_class = class_create(THIS_MODULE, "demo_chrdev_class");
    if (IS_ERR(demo_class))
        return PTR_ERR(demo_class);
    // 2. 在class里边创建一个设备叫xxx，然后mdev自动创建设备节点/dev/xxx
    //  在/dev目录下创建相应的设备节点
    demo_class_devs = class_device_create(demo_class, NULL, MKDEV(major, 0), NULL, "demo_cdev_nodeName"); 
    if (unlikely(IS_ERR(demo_class_devs)))
        return PTR_ERR(demo_class_devs);

    return 0;
}
// 出口函数
static void demo_cdev_exit(void)
{
    unregister_chrdev(major, "demo_chrdev_1");
    // 对应卸载
    class_device_unregister(demo_class_devs);  
    class_destroy(demo_class);
}

// 内核模块相关
module_init(demo_cdev_init);
module_exit(demo_cdev_exit);
MODULE_LICENSE("GPL");