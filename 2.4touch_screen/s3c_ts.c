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
static struct timer_list ts_timer; // 用于处理需要多次中断的 ts滑动长按

// 进入等待松开中断模式
static void enter_wait_pen_up_mode(void)
{
    adcregs->adctsc = 0x1d3; // bit8 1 = Detect Stylus Up Interrupt Signal.
}
// 进入等待按下中断模式
static void enter_wait_pen_down_mode(void)
{
    adcregs->adctsc = 0xd3;  // bit8 0 = Detect Stylus Down Interrupt Signal
}
// 使adc进入 Auto(Sequential) X/Y Position Conversion Mode
static void enter_measure_xy_mode(void)
{
    //      为啥不是 |= 可能其它位自动设置吧 
    adcregs->adctsc = (1<<3) | (1<<2);  // 1 = Auto Sequential measurement of X-position, Y-position.
}
// 启动adc，开始转换
static void start_adc(void)
{
    adcregs->adccon |= (1<<0);
}

// 根据x y多次采样得到的数组计算正常的情况
//  1 2 3 4  3与1、2平均值的差值应该<ERR_LIMIT && 
//           4与2、3平均值的差值应该<ERR_LIMIT
static int ts_filter(const int x[], const int y[])
{
#define ERR_LIMIT   10 // 像素误差极限
    int avr_x, avr_y;
    int dst_x, dst_y;
    avr_x = (x[0] + x[1]) / 2;
    avr_y = (x[0] + x[1]) / 2;
    dst_x = x[2] > avr_x ? x[2] - avr_x : avr_x - x[2];
    dst_y = y[2] > avr_y ? y[2] - avr_y : avr_y - y[2];
    if (dst_x > ERR_LIMIT || dst_x > ERR_LIMIT) // 3与1、2
        return 0;

    avr_x = (x[1] + x[2]) / 2;
    avr_y = (x[1] + x[3]) / 2;
    dst_x = x[3] > avr_x ? x[3] - avr_x : avr_x - x[3];
    dst_y = y[3] > avr_y ? y[3] - avr_y : avr_y - y[3];
    if (dst_x > ERR_LIMIT || dst_x > ERR_LIMIT) // 4与2、3
        return 0;
    
    return 1;   // 符合正确情况
}
// 定时时间到 处理函数
static void ts_timer_function(unsigned long data)
{
    if (adcregs->adcdat0 & (1<<15)) { // 已经松开
        // 等待下次按下
        enter_wait_pen_down_mode();
    } else {                          // 依然是按下
        enter_measure_xy_mode(); // 进入测量模式
        start_adc();           
    }
}

// adc采集完成会有进入adc isr
static irqreturn_t adc_irq(int irq, void *dev_id)
{
    static int cnt = 0;
    static int x[4], y[4]; // 保存4次结果 求均值用

    // 优化措施4:  自定义过滤异常的值 ts_filter （之前的优化也在）
    if (adcregs->adcdat0 & (1<<15)) { // 松开
        cnt = 0;
        // 上报事件
        input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0); // 多级压力 1表示按下
        input_report_key(s3c_ts_dev, BTN_TOUCH, 0); // 1表示按下        
        input_sync(s3c_ts_dev); // 上报完毕
        // 等待下次按下
        enter_wait_pen_down_mode();
    } else {
        x[cnt] = adcregs->adcdat0 & (0x3ff);
        y[cnt] = adcregs->adcdat1 & (0x3ff);
        ++cnt;
        if (cnt == 4) {
            cnt = 0;
            if (ts_filter(x, y)) { // 自定义过滤方法filter
                // 测量值放在 ADCDAT0  ADCDAT1 打印电压测量值
                // printk("irq cnt=%d, x=%d, y=%d\n", ++cnt, \
                //         (x[0]+x[1]+x[2]+x[3])/4, \ 
                //         (y[0]+y[1]+y[2]+y[3])/4); 
                // 上报事件 绝对位移 压力 
                input_report_abs(s3c_ts_dev, ABS_X, (x[0]+x[1]+x[2]+x[3])/4);
                input_report_abs(s3c_ts_dev, ABS_Y, (y[0]+y[1]+y[2]+y[3])/4);
                input_report_abs(s3c_ts_dev, ABS_PRESSURE, 1); // 多级压力 1表示按下
                input_report_key(s3c_ts_dev, BTN_TOUCH, 1);    // 1表示按下
                input_sync(s3c_ts_dev); // 上报完毕
                // 测量完 打印之后等待松开，否则测量一次，不明白为什么？？？
                enter_wait_pen_up_mode();

                // 启动定时器
                mod_timer(&ts_timer, jiffies+HZ/100); // 10ms
            }
        } else {
            enter_measure_xy_mode(); // 进入测量模式
            start_adc();            
        }
    }    

	return IRQ_HANDLED;
}
// 触摸按下松开进入中断函数
static irqreturn_t pen_down_up_irq(int irq, void *dev_id)
{
    printk("pen down/up.\n");
    // 分辨按下还是松开 判断adcdat0 bit15
    if (adcregs->adcdat0 & (1<<15)) { // 松开
        // printk("pen_up.\n");
        // 上报事件
        input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0); // 多级压力 1表示按下
        input_report_key(s3c_ts_dev, BTN_TOUCH, 0);  
        input_sync(s3c_ts_dev); // 上报完毕       
        enter_wait_pen_down_mode(); // 进入等待按下模式
    } else {                          // 按下
        // printk("pen_down.\n");    
        // enter_wait_pen_up_mode();   // 进入等待松开模式
        enter_measure_xy_mode(); // 进入测量模式
        start_adc(); // 启动adc开始转换，转换需要时间，但是不等，
                     // 设置了adc中断，转换完成后会进入adc_irq
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
    
    // 注册中断 中断号 触摸屏中断 isr 。。。
    request_irq(IRQ_TC, pen_down_up_irq, IRQF_SAMPLE_RANDOM, \
                "ts_pen", NULL);
    // 注册ADC中断
    request_irq(IRQ_ADC, adc_irq, IRQF_SAMPLE_RANDOM, \
                "adc", NULL);
    
    // 优化措施1: 延时测量，配置延时寄存器ADCDLY
    // 使得电压稳定后再发出IRQ_TC中断
    adcregs->adcdly = 0xffff; // 延时最大值
    
    // 优化措施5：用定时器处理滑动长按
    //  
    init_timer(&ts_timer);
    ts_timer.function = ts_timer_function;
    add_timer(&ts_timer);

    // 进入等待中断模式
    enter_wait_pen_down_mode();// 进入等待按下中断模式

    return 0;
}
static void s3c_ts_exit(void)
{
    del_timer(&ts_timer);
    free_irq(IRQ_TC, NULL);
    free_irq(IRQ_ADC, NULL);
    adcregs = iounmap(adcregs);
    input_unregister_device(s3c_ts_dev);
    input_free_device(s3c_ts_dev);
}
module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");

// 按下触摸屏，
// 发生IRQ_TC中断，设置成测量模式并启动ADC，
// ADC转换完成发生中断，各种异常值处理优化措施，
//      如果4次判断都是按下 则上报事件 并 则启动定时器
// 定时时间到若还是按下则测量并启动ADC转换，不是按下则等待下次按下