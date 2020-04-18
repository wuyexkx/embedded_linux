#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/fb.h>

static struct fb_info *s3c_lcd_fbinfo;
static int lcd_init(void)
{
    // 1. 分配一个fb_info
    //   内核结构分配小技巧，可以多分配些空间，
    //   结构内部有指针指向这段空间，私有空间，
    // |====|=|。 这里不需要额外空间所以0
    s3c_lcd_fbinfo = framebuffer_alloc(0, NULL);
    // 2. 设置 分配的结构体
    // 2.1设置固定的参数
    // 2.2设置可变的参数
    // 2.3设置fb_ops
    // 其他设置

    // 3. 硬件相关操作
    // 3.1配置GPIO用于LCD
    // 3.2根据LCD手册设置LCD控制器，VCLK的频率等
    // 3.3分配显存(framebuffer),把地址告诉LCD控制器
    //      从内存中划出，这里没有实际显存

    // 4. 注册
    register_framebuffer(s3c_lcd_fbinfo);
    return 0;
}
static void lcd_exit(void)
{

}
module_init(lcd_init);
module_exit(lcd_exit);
MODULE_LICENSE("GPL");