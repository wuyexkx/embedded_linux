#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h> // S3C2410_GPFx

#include <asm/gpio.h>

// 引脚描述结构体 中断号、名称、管脚、键值
static struct pin_desc {
    int irq;
    char *name;
    unsigned int pin;
    unsigned int key_val;
};
// 定义一个引脚描述数组，并初始化，对应于4个按键的信息
#define NUMS_KEYS   4
struct pin_desc pins_desc[NUMS_KEYS] = {
    {IRQ_EINT0,  "EINT0_S2",  S3C2410_GPF0,  KEY_L},
    {IRQ_EINT2,  "EINT2_S3",  S3C2410_GPF2,  KEY_S},
    {IRQ_EINT11, "EINT11_S4", S3C2410_GPG3,  KEY_ENTER},
    {IRQ_EINT19, "EINT19_S5", S3C2410_GPG11, KEY_LEFTSHIFT}
}

// 定义一个输入设备结构
static struct input_dev *_buttons_dev;
// 定义一个定时器
static struct timer_list buttons_timer;
// 发生中断后，参数的在irq和timer_function之间传递
static struct pin_desc *irq_pd; // 在中断中的引脚描述pin_desc

// ISR 发生按键中断之后会调用这个函数, irq就是对应的中断号
// 按键中断发生后进入irq，修改定时器溢出值，修改按键的struct
static irqreturn_t buttons_irq(int irq, void *dev_id)
{
    irq_pd = (struct pin_desc *)dev_id;
    // jiffies是全局变量,每隔10ms系统产生一个时钟中断, 10ms
    mod_timer(&buttons_timer, jiffies+HZ/100);

    return IRQ_RETVAL(IRQ_HANDLED);
}
// 时间处理函数，超时会被调用
static void buttons_timer_function(unsigned long data)
{   
    struct pin_desc * pindesc = irq_pd;
    unsigned int pin_status;
    if (!pindesc)
        return;
    // 获取该引脚状态
    pin_status = s3c2410_gpio_getpin(pindesc->pin);
    // 上报事件
    // 之前的代码是 确定按键值，唤醒用户程序或者发信号，
    // 在input subsystem中，现在只需要上报事件就ok
    // input_event会从input_dev的h_list中找到input_hanldle
    //  找到handler调用event函数 在drivers\input\input.c中input_event的最后几行    
    if (pin_status) { // 松开
        // 哪个设备、事件类型、哪个事件、值（0表示松开，1表示按下）
        input_event(_buttons_dev, EV_KEY, pindesc->key_val, 0);
        // 还有上报同步事件
        input_sync(_buttons_dev);
    } else {          // 按下
        input_event(_buttons_dev, EV_KEY, pindesc->key_val, 1);
        input_sync(_buttons_dev);
    }
}

static int buttons_inSubSys_init(void)
{
    int i=0, error;
    /*1. 分配一个input_dev结构体 */
    _buttons_dev = input_allocate_device();
    if (!_buttons_dev)
        return -ENOMEM;
    /*2. 设置 */
	// unsigned long evbit[NBITS(EV_MAX)];  // 表示能产生哪类事件
	// unsigned long keybit[NBITS(KEY_MAX)];// 表示能产生哪些事件
	// unsigned long relbit[NBITS(REL_MAX)];// 表示能产生哪些相对位移事件 (x y 滚轮)
  
    // #define EV_SYN			0x00 // 同步类事件
    // #define EV_KEY			0x01 // 按键类事件
    // #define EV_REL			0x02 // 相对位移类事件 鼠标滑动
    // #define EV_ABS			0x03 // 绝对位移类事件 点击屏幕
    // 2.1能产生哪类事件，按键类 重复类（修改定时器超时事件）
    set_bit(EV_KEY, _buttons_dev->evbit);
    set_bit(EV_REP, _buttons_dev->evbit); // 只需要这一句就可以实现按下按键时重复按键值
    // 2.2能产生这类事件里的哪些事件 (哪个按键呢 l s ENTER LEFTSHIFT)
    set_bit(KEY_L, _buttons_dev->keybit); // 设置数组的某一位
    set_bit(KEY_S, _buttons_dev->keybit);
    set_bit(KEY_ENTER, _buttons_dev->keybit);
    set_bit(KEY_LEFTSHIFT, _buttons_dev->keybit);
    /*3. 注册 */ 
    // 加入到某个管理输入子系统设备的链表，然后依次与右边的hanlder的id比较，
    // 看是否支持此设备，支持则调用connect函数，建立input_handle, 
    // dev指向左，handler指向右边，再把input_handle放入两边的.h_list
    error = input_register_device(_buttons_dev);
    if (error){
		printk(KERN_ERR "Unable to register _buttons_dev input device.\n");
        goto fail_input_register_device;
    }
    /*4. 硬件相关操作 */
    // 初始化定时器、定时器函数、start a timer，用于防抖动
    init_timer(&buttons_timer);
    buttons_timer.function = buttons_timer_function;
    add_timer(&buttons_timer);
    // 注册中断函数
    for (i=0; i<NUMS_KEYS; ++i){
        // 应该需要判断返回值
        request_irq(pins_desc[i].irq, buttons_irq, IRQT_BOTHEDGE, \
                    pins_desc[i].name, &pins_desc[i]);
		if (error) {
			printk(KERN_ERR "gpio-keys: unable to claim irq %d; error %d.\n",
				pins_desc[i].irq, error);
			goto fail_request_irq;        
        }

    return 0;

// 倒序释放
fail_request_irq:
    // 对应于request_irq，注意i
    for (int i=i-1; i>=0; --i)     
        free_irq(pins_desc[i].irq, &pins_desc[i]);
    // 对应于add_timer
    del_timer(&buttons_timer);

fail_input_register_device:
    // 对应于input_register_device
    input_unregister_device(_buttons_dev);
    // input_allocate_device
    input_free_device(_buttons_dev);

    return error;
}

static void buttons_inSubSys_exit(void)
{
    int i;
    // 对应于request_irq
    for (i=NUMS_KEYS-1; i>=; --i) {
        free_irq(pins_desc[i].irq, &pins_desc[i]);
    }    
    // 对应于add_timer
    del_timer(&buttons_timer);
    // 对应于input_register_device
    input_unregister_device(_buttons_dev);
    // input_allocate_device
    input_free_device(_buttons_dev);
}

module_init(buttons_inSubSys_init);
module_exit(buttons_inSubSys_exit);
MODULE_LICENSE("GPL");
