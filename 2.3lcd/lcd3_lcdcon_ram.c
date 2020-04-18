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

// s3c的LCD寄存器
struct s3c_lcd_regs {
    unsigned long lcdcon1;
    unsigned long lcdcon2;
    unsigned long lcdcon3;
    unsigned long lcdcon4;
    unsigned long lcdcon5;
    unsigned long lcdsaddr1;
    unsigned long lcdsaddr2;
    unsigned long lcdsaddr3;
    unsigned long redlut;
    unsigned long greenlut;
    unsigned long bluelut;
    unsigned long reserved[9]; // 连续的4字节，这里不连续，空出来
    unsigned long dithmode;
    unsigned long tpal;
    unsigned long lcdintpnd;
    unsigned long lcdsrcpnd;
    unsigned long lcdintmsk;
    unsigned long tconsel;
};
// s3c的LCD引脚设置
static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *gpccon;
static volatile unsigned long *gpdcon;
static volatile unsigned long *gpgcon;
static volatile struct s3c_lcd_regs *lcd_regs;

static struct fb_info *s3c_lcd_fbinfo;
static struct fb_ops s3c_lcdfb_ops = {
	.owner		= THIS_MODULE,
	// .fb_setcolreg	= atmel_lcdfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,    
};

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
    // 2.4其他设置
    // s3c_lcd_fbinfo->pseudo_palette  = ; // 假调色板16种颜色 Fake palette of 16 colors 
    // s3c_lcd_fbinfo->screen_base     = ; // 显存的虚拟地址 如果没提供write就用到screen_base
    s3c_lcd_fbinfo->screen_size     = 240*320*16/8; // Amount of ioremapped VRAM or 0
    // 3. 硬件相关操作
    // 3.1配置GPIO用于LCD
    gpbcon = ioremap(0x56000010, 8); // 一页一页的映射 1024字节
    gpbdat = gpbcon + 1;
    gpccon = ioremap(0x56000020, 4); // 4和8一样 一页一页的映射 1024字节
    gpdcon = ioremap(0x56000030, 4);
    gpgcon = ioremap(0x56000060, 4);

    *gpccon = 0xaaaaaaaa; // GPIO管脚用于VD[7:0] LCDVF[2:0] VM VFRAME VLINE LEND
    *gpdcon = 0xaaaaaaaa; // GPIO管脚用于VD[23:8]

    *gpbcon &= ~(3);      // 设置GPB0为输出引脚 背光
    *gpbcon |= 1;
    *gpbdat &= ~1;        // 输出低电平

    *gpgcon |= (3<<8);    // GPG4用作LCD_PWREN LCD电源使能引脚
    // 3.2根据LCD手册设置LCD控制器，VCLK的频率等
    lcd_regs = ioremap(0x4d000000, sizeof(lcd_regs)); // 寄存器组映射
        // bit[17:8]: TFT: VCLK = HCLK / [(CLKVAL+1) x 2] (CLKVAL‡ 0)
        //                  VCLK(扫描时钟)<=10MHz  HCLK(内核时钟频率) 100MHz
        //                  10MHz(100ns)=100MHz/[(CLKVAL+1)x2]-->CLKVAL=4
        // bit[6:5] : 11 = TFT LCD panel
        // bit[4:1] : 1100 = 16 bpp for TFT. BPP (Bits Per Pixel) mode.
        // bit[0]   : 0 = Disable the video output and the LCD control signal.
    lcd_regs->lcdcon1 = (4<<8) | (3<<5) | (0x0C<<1) | (0<<0);
        // 垂直方向的时间参数
        // bit[31:24]: VBPD VSYNC过多长时间后才能发出第一行数据
        //                  LCD手册上T0-T2-T1=4 所以VBPD = 3
        // bit[23:14]: LINEVAL 多少行，320 所以LINEVAL = 319
        // bit[13:6] : VFPD 发出最后一行数据后过多久才发出VSYNC信号
        //                  LCD手册T2-T5=322-320=2， 所以=1
        // bit[5:0]  : VSYNC VSYNC信号脉冲宽度，LCD手册T1=1 所以=0
    lcd_regs->lcdcon2 = (3<<24) | (319<<14) | (1<<6) | (0<<0);
        // 水平方向的时间参数
        // bit[25:19]: HBPD HSYNC过多长时间后才能发出第一个像素数据
        //                  LCD手册 T6-T7-T8=17 所以16
        // bit[18:8] : HOZVAL 多少列 240 所以239
        // bit[7:0]  : HFPD 发出最后一行最后一个像素后过多久才发出HSYNC信号
        //                  LCD手册 T8-T11=11 所以10
    lcd_regs->lcdcon3 = (16<<19) | (239<<8) | (10<<0);
        // bit[7:0]  : HSPW HSYNC的脉冲宽度 LCD手册T7=5 所以4 
    lcd_regs->lcdcon4 = (4<<0);
        // 信号的极性
        // bit[11]: 1=565 Format
        // bit[10]: 0 = The video data is fetched at VCLK falling edge
        // bit[9] : 1 反转HSYNC 低电平有效
        // bit[8] : 1 反转VSYNC 低电平有效
        // bit[6] : 0 VDEN不用反转
        // bit[3] : 0 PWREN输出0
        // bit[1] : 0 BSWP
        // bit[0] : 1 HWSWP 2440手册P413
    lcd_regs->lcdcon5 = (1<<11) | (1<<9) | (1<<8)| (1<<0);
    // 3.3分配显存(framebuffer),把地址告诉LCD控制器
    //      从内存中划出，这里没有实际显存,需要连续内存
    //      返回虚拟地址
    s3c_lcd_fbinfo->screen_base = dma_alloc_writecombine(NULL, \
                    s3c_lcd_fbinfo->fix.smem_len, \
                    s3c_lcd_fbinfo->fix.smem_start, GFP_KERNEL);
        // 告诉lcd控制器 显存的起始地址
    lcd_regs->lcdsaddr1 = (s3c_lcd_fbinfo->fix.smem_start>>1) & ~(3<<30); // 0~29bit 30 31清0
        // 告诉lcd控制器显存的结束地址
    lcd_regs->lcdsaddr2 = ((s3c_lcd_fbinfo->fix.smem_start+s3c_lcd_fbinfo->fix.smem_len)>>1) & \
                        0x1fffff; // 0~20bit
        // 一行显示的长度（单位：半字）
    lcd_regs->lcdsaddr3 = 240*16/16;
    // s3c_lcd_fbinfo->fix.smem_start = ; // 显存的物理地址
    // 启动LCD 
    lcd_regs->lcdcon1 |= (1<<0); // 使能LCD
    *gpbdat |= 1;                // 打开背光
        
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