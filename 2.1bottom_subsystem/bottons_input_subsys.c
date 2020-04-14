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


static input
static int bottons_inSubSys_init(void)
{
    /*1. 分配一个input_dev结构体 */

    /*2. 设置 */

    /*3. 注册 */

    /*4. 硬件相关操作 */

    return 0;
}

static void bottons_inSubSys_exit(void)
{

}

module_init(bottons_inSubSys_init);
module_exit(bottons_inSubSys_exit);
MODULE_LICENSE("GPL");
