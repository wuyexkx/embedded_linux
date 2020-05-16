#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <asm/io.h>
// 参考drivers\mtd\maps\physmap.c。

static struct map_info *normap_info;
static struct mtd_info *normtd_info;
static struct mtd_partition nor_part[] = { // 用于nor分区的数组
	[0] = {
        .name   = "bootloader_nor",
        .size   = 0x00040000,
		.offset	= 0,
	},
	[1] = {
        .name   = "root_nor",
        .offset = MTDPART_OFS_APPEND,
        .size   = MTDPART_SIZ_FULL, // 剩下的所有都是该分区
	}
};

static int s3c_nor_init(void)
{
    // 1.分配map_info结构体
    normap_info = kzalloc(sizeof(struct map_info), GFP_KERNEL);
    // 2.设置 物理基地址(phys)、大小(size)、位宽(bankwidth) 虚拟基地址(virt)
    //  内核看到的不同开发板最小的差别就是这些了
	normap_info->name = "s3c_nor";
	normap_info->phys = 0;          // nor启动时 物理地址为0
	normap_info->size = 0x1000000;  // 16M 一定>=nor的实际大小
	normap_info->bankwidth = 2;     // 2*Byte=16bit
	normap_info->virt = ioremap(normap_info->phys, normap_info->size);

	simple_map_init(&normap_info);  // 简单初始化

    // 3.使用 调用nor flash的协议层函数来识别
	normtd_info = do_map_probe("cfi_probe", &normap_info);
    if (normap_info)    // 这里只尝试两种模式cfi和jedec
        printk("use cfi_probe.\n");
    else {
        printk("use jedec_probe.\n");
	    normtd_info = do_map_probe("jedec_probe", &normap_info);
    }
    if (!normap_info) { // 不是这两种模式
        printk("mode cfi_probe or jedec_probe failed.\n");
        iounmap(normap_info->virt);
        kfree(normap_info);
        return -EIO;    // I/O error
    }
    // 4.添加分区，不是必须。add_mtd_partitions
    add_mtd_partitions(normtd_info, nor_part, 2);
    return 0;
}

static void s3c_nor_exit(void)
{
    del_mtd_partitions(normtd_info);
    iounmap(normap_info->virt);
    kfree(normap_info);
}
module_init(s3c_nor_init);
module_exit(s3c_nor_exit);
MODULE_LICENSE("GPL");
