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
static struct fb_ops s3c_lcdfb_ops = {
	.owner		= THIS_MODULE,
	// .fb_setcolreg	= atmel_lcdfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,    
}
static int lcd_init(void)
{
    // 1. 分配一个fb_info
    //   内核结构分配小技巧，可以多分配些空间，
    //   结构内部有指针指向这段空间，私有空间，
    // |====|=|。 这里不需要额外空间所以0
    s3c_lcd_fbinfo = framebuffer_alloc(0, NULL);
    // 2. 设置 分配的结构体，
    // 2.1设置固定的参数 未设置的分配之后都是0
    strcpy(s3c_lcd_fbinfo->fix.id, "mylcd");    // id name
    s3c_lcd_fbinfo->fix.smem_len    = 240*320*16/8;// 显存的长度 16=5+6+5
    s3c_lcd_fbinfo->fix.type        = FB_TYPE_PACKED_PIXELS;// see FB_TYPE_*
        // s3c_lcd_fbinfo->fix.type_aux = 平板会用到
    s3c_lcd_fbinfo->fix.visual      = FB_VISUAL_TRUECOLOR; // TFT屏幕是真彩 MONO单色
    s3c_lcd_fbinfo->fix.line_length = 240*16/8; // 一个像素由16位表示就是2Bytes，length of a line in bytes
    // 2.2设置可变的参数
    s3c_lcd_fbinfo->var.xres         = 240; // 实际分辨率
    s3c_lcd_fbinfo->var.yres         = 320;
    s3c_lcd_fbinfo->var.xres_virtual = 240; // 可调整的分辨率
    s3c_lcd_fbinfo->var.yres_virtual = 320;
    s3c_lcd_fbinfo->var.bits_per_pixel = 16;// 2440不支持18位，丢弃2位
        // RGB 565 R:11 G:5 B:0
    s3c_lcd_fbinfo->var.red.offset   =  11;// red对应位的区域
    s3c_lcd_fbinfo->var.red.length   =  5; // red长度
    s3c_lcd_fbinfo->var.green.offset =  5;
    s3c_lcd_fbinfo->var.green.length =  6; 
    s3c_lcd_fbinfo->var.blue.offset  =  0;
    s3c_lcd_fbinfo->var.blue.length  =  5; 

    s3c_lcd_fbinfo->var.activate     =  FB_ACTIVATE_NOW;   
    // 2.3设置fb_ops. 参考drivers\video\atmel_lcdfb.c
    s3c_lcd_fbinfo->fbops = &s3c_lcdfb_ops;
    // 其他设置
    // s3c_lcd_fbinfo->pseudo_palette  = ; // 假调色板16种颜色 Fake palette of 16 colors 
    // s3c_lcd_fbinfo->screen_base     = ; // 显存的虚拟地址 如果没提供write就用到screen_base
    s3c_lcd_fbinfo->screen_size     = 240*320*16/8; // Amount of ioremapped VRAM or 0
    // 3. 硬件相关操作
    // 3.1配置GPIO用于LCD
    // 3.2根据LCD手册设置LCD控制器，VCLK的频率等
    // 3.3分配显存(framebuffer),把地址告诉LCD控制器
    //      从内存中划出，这里没有实际显存
    s3c_lcd_fbinfo->fix.smem_start = ; // 显存的物理地址

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