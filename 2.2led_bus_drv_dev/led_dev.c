#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>

// 分配  设置  注册一个platform_device

#define DEVICE_NAME  "myled"

// 资源描述，寄存器，地址
static struct resource led_resource[] = {
    [0] = {
        .start = 0x56000050,        // 起始地址
        .end   = 0x56000050 + 8 - 1,// 结束地址
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = 4,
        .end   = 4,
        .flags = IORESOURCE_IRQ,
    }
};
// 先不写，硬件相关代码
static void led_release(struct device *dev)
{

}
static struct platform_device led_dev = {
    .name           = DEVICE_NAME,
    .id             = -1,
    .num_resources  = ARRAY_SIZE(led_resource),
    .resource       = led_resource,
    .dev.release    = led_release, // platform_device内有dev内有release。 硬件相关代码
};

static int led_dev_init(void)
{
    platform_device_register(&led_dev); // 最终放到platform的dev链表中 device_add
    return 0;
}
static void led_dev_exit(void)
{
    platform_device_unregister(&led_dev);
}
module_init(led_dev_init);
module_exit(led_dev_exit);
MODULE_LICENSE("GPL");