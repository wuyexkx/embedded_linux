#include <linux/init.h>     // module_init module_exit
#include <linux/module.h>   // MODULE_LICENSE
#include <linux/fs.h>       // file_operations
#include <linux/cdev.h>     // cdev
#include <linux/kernel.h>
#include <linux/device.h>   // class_device
#include <asm/uaccess.h>    // copy_from_user copy_to_user
#include <asm/io.h>         // ioremap  iounmap

// 1 确定主设备号
static int major;

// 2 构造file_operations 
static struct file_operations chrdev_fops = {

}

static int chrdev_init(void)
{

    return 0;
    
}

static void chrdev_exit(void)
{

}

module_init(chrdev_init);
module_exit(chrdev_exit);
MODULE_LICENSE();
