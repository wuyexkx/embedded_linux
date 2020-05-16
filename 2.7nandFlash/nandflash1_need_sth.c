#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

#include <asm/arch/regs-nand.h>
#include <asm/arch/nand.h>

static struct nand_chip *s3c_nand;
static struct mtd_info *s3c_mtd;

// nand片选
static void s3c2440_select_chip(struct mtd_info *mtd, int chipnr)
{
    // 片选
    if (chipnr == -1) { 
        // 取消选中芯片 NFCONT[1] 为0

    } else { 
        // 选中芯片 NFCONT[1] 为1

    }
}
// 发命令或者地址
static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
    if (ctrl & NAND_CLE) { // 发命令
        // NFCMMD = dat
    } else {               // 发地址
        // NFADDR = dat
    }
}
// 判断状态
static int s3c2440_dev_ready(struct mtd_info *mtd)
{
    // 返回NFSTAT的bit0
    return ;
}

static int s3c_nand_init(void)
{
    // 1.分配一个nand_chip结构体
	s3c_nand = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
    // 2.设置nand_chip, nand_chip是给nand_scan函数用，
        // 可以先看nand_scan怎么用nand_chip的，再去设置
        // nand_chip应该提供 选中、发命令、地址、数据，读数据、判断状态 功能
    s3c_nand->select_chip = s3c2440_select_chip;// 默认的选择函数不能用需要自己写
    s3c_nand->cmdfunc   = s3c2440_cmd_ctrl;     // 默认的会调用nand_chip->cmdfunc，这里也需要构造
    s3c_nand->IO_ADDR_R = 0;                    // 读取数据 NFDATA的虚拟地址 
    chip->IO_ADDR_W = ;                         // 写入数据
    chip->dev_ready = s3c2440_dev_ready;        // 判断状态
    // 3.硬件相关操作
    // 4.使用 nand_scan，nand_scan识别nand，构造mtd_info（包含读写擦除等方法）
    s3c_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
    s3c_mtd->priv   = s3c_nand; // mtd与nand_chip联系
    s3c_mtd->owner  = THIS_MODULE; 
    nand_scan(s3c_mtd, 1); // 1个芯片

    // 5.add_mtd_partitions

    return 0;
}
static void s3c_nand_exit(void)
{

}
module_init(s3c_nand_init);
module_exit(s3c_nand_exit);
MODULE_LICENSE("GPL");
