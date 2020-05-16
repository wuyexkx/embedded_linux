#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <asm/plat-s3c24xx/ts.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>


struct adc_regs {
    unsigned long adccon;  
    unsigned long adctsc;
    unsigned long adcdly;
    unsigned long adcdat0; 
    unsigned long adcdat1; 
    unsigned long adcupdn; 
};

static struct input_dev * s3c_ts_dev;
static volatile struct adc_regs *adcregs;

// 进入等待松开模式
static void enter_wait_pen_up_mode(void)
{
    adcregs->adctsc = 0x1d3; // bit8 1 = Detect Stylus Up Interrupt Signal.
}
// 进入等待按下模式
static void  enter_wait_pen_down_mode(void)
{
    adcregs->adctsc = 0xd3;  // bit8 0 = Detect Stylus Down Interrupt Signal
}
// 触摸按下松开进入中断函数
static irqreturn_t pen_down_up_irq(int irq, void *dev_id)
{
    printk("pen down/up.\n");
    // 分辨按下还是松开 判断adcdat0 bit15
    if (adcregs->adcdat0 & (1<<15)) { // 松开
        printk("pen_up.\n");
        enter_wait_pen_down_mode(); // 进入等待按下模式
    } else {                          // 按下
        printk("pen_down.\n");    
        enter_wait_pen_up_mode();   // 进入等待松开模式
    }

	return IRQ_HANDLED;
}

static int s3c_ts_init(void)
{
    struct clk * adc_clock; // adc的时钟，需要打开，内核上电关闭了

    // 1.分配input_dev结构体
    s3c_ts_dev = input_allocate_device();
    // 2.设置该结构体
    // 2.1能产生哪类事件
    set_bit(EV_KEY, s3c_ts_dev->evbit);
    set_bit(EV_ABS, s3c_ts_dev->evbit);    // 绝对位置 
    // 2.2能产生这类事件的哪些事件
    set_bit(BTN_TOUCH, s3c_ts_dev->evbit); // 点击，类button
    // void input_set_abs_params(input_dev *, int axis, int min, int max, int fuzz, int flat)
    //  ADC最大值最小值
	input_set_abs_params(s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);
    // 3.注册
    input_register_device(s3c_ts_dev);
    // 4.硬件相关操作
    // 4.1使能adc时钟 CLKCON[15]
    adc_clock = clk_get(NULL, "adc");
	clk_enable(adc_clock);  // 打开时钟    
    // 4.2设置s3c的ADC/TS寄存器
    adcregs = ioremap(0x58000000, sizeof(struct adc_regs));
        // bit[14]  : 1 将pclck预分配
        // bit[13:6]: 分频系数 49
        //      ADCCLK=PCLK/(49+1)=50/(49+1)=1MHz
        // bit[0]   : 0 转换开始信号，现在不设置
    adcregs->adccon = (1<<14) | (49<<6);
    
    // 注册中断 中断号 isr 。。。
    request_irq(IRQ_TC, pen_down_up_irq, IRQF_SAMPLE_RANDOM, \
                "ts_pen", NULL);
    // 进入等待中断模式
    enter_wait_pen_down_mode();// 进入等待按下模式

    return 0;
}
static void s3c_ts_exit(void)
{
    free_irq(IRQ_TC, NULL);
    adcregs = iounmap(adcregs);
    input_unregister_device(s3c_ts_dev);
    input_free_device(s3c_ts_dev);
}
module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");
