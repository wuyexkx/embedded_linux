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

struct s3c2440_nand_regs {
    unsigned long nfconf  ; // 0x4E000000
    unsigned long nfcont  ; // 0x4E000004
    unsigned long nfcmd   ; // 0x4E000008
    unsigned long nfaddr  ; // 0x4E00000C
    unsigned long nfdata  ; // 0x4E000010
    unsigned long nfmecc0 ; // 0x4E000014
    unsigned long nfmecc1 ; // 0x4E000018
    unsigned long nfsecc  ; // 0x4E00001C
    unsigned long nfstat  ; // 0x4E000020
    unsigned long nfestat0; // 0x4E000024
    unsigned long nfestat1; // 0x4E000028
    unsigned long nfmecc0 ; // 0x4E00002C
    unsigned long nfmecc1 ; // 0x4E000030
    unsigned long nfsecc  ; // 0x4E000034
    unsigned long nfsblk  ; // 0x4E000038
    unsigned long nfeblk  ; // 0x4E00003C
};
static volatile struct s3c2440_nand_regs *s3c_nand_regs;
static struct nand_chip *s3c_nand;
static struct mtd_info *s3c_mtd;

// nand片选
static void s3c2440_select_chip(struct mtd_info *mtd, int chipnr)
{
    // 片选
    if (chipnr == -1) { 
        // 取消选中芯片 NFCONT[1] 为1
        s3c_nand_regs->nfcont |= (1<<1);
    } else { 
        // 选中芯片 NFCONT[1] 为0
        s3c_nand_regs->nfcont &= ~(1<<1);
    }
}
// 发命令或者地址
static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
    if (ctrl & NAND_CLE) { // 发命令
        // NFCMMD = dat
        s3c_nand_regs->nfcmd  = dat;
    } else {               // 发地址
        // NFADDR = dat
        s3c_nand_regs->nfcmd  = dat;
    }
}
// 判断状态
static int s3c2440_dev_ready(struct mtd_info *mtd)
{
    // 返回NFSTAT的bit0
    return (s3c_nand_regs->nfstat & (1<<0));
}

static int s3c_nand_init(void)
{
    struct clk *nand_clk;
    // 1.分配一个nand_chip结构体
	s3c_nand = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
    // 3.硬件相关操作 nand寄存器地址映射
    s3c_nand_regs = ioremap(0x4E000000, sizeof(struct s3c2440_nand_regs));    
    // 2.设置nand_chip, nand_chip是给nand_scan函数用，
        // 可以先看nand_scan怎么用nand_chip的，再去设置
        // nand_chip应该提供 选中、发命令、地址、数据，读数据、判断状态 功能
    s3c_nand->select_chip = s3c2440_select_chip;// 默认的选择函数不能用需要自己写
    s3c_nand->cmdfunc   = s3c2440_cmd_ctrl;     // 默认的会调用nand_chip->cmdfunc，这里也需要构造
    s3c_nand->IO_ADDR_R = s3c_nand_regs->nfdata;// 读取数据 NFDATA的虚拟地址 
    s3c_nand->IO_ADDR_W = s3c_nand_regs->nfdata;// 写入数据
    s3c_nand->dev_ready = s3c2440_dev_ready;    // 判断状态
    // 3.硬件相关操作 电平维持时间
        // 使能nand控制器的时钟，后面才能读写寄存器
	nand_clk = clk_get(NULL, "nand");
	clk_enable(nand_clk); // CLKCON的bit4
        // HCLK=100MHz, 10ns
        // TACLS: 发出CLE/ALE之后隔多久发出nWE信号，
        //  从nand手册计算得知CLE/ALE与nWE可以同时，所以TACLS=0
        // TWRPH0: nWE的脉冲宽度，HCLK x ( TWRPH0 + 1 )
        //  从nand手册知>=12ns,所以TWRPH0=1
        // TWRPH1: nWE变为高电平后CLE/ALE才能变为低电平，
        //  从nand手册知>=5ns，所以TWRPH1>=0
#define TACLS   0
#define TWRPH0  1
#define TWRPH1  0
    s3c_nand_regs->nfconf = (TACLS<<12) | (TWRPH0<<8) | (TWRPH1<<4);
        // NFCONF 取消片选，使能nand控制器 
        //  bit1为1 取消片选
        //  bit0为1 使能nand控制器
    s3c_nand_regs->nfcont = (1<<1) | (1<<0);
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
    kfree(s3c_mtd);
    iounmap(s3c_nand_regs);    
    kfree(s3c_nand);
}
module_init(s3c_nand_init);
module_exit(s3c_nand_exit);
MODULE_LICENSE("GPL");