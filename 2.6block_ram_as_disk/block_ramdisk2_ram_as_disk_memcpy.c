#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/dma.h>

static struct gendisk *ramblock_disk;
static request_queue_t *ramblock_queue; // 队列
static DEFINE_SPINLOCK(ramblock_lock);  // 自旋锁
static int major;
static struct block_device_operations ramdisk_fops = {
	.owner	= THIS_MODULE,
};
#define RAMBLOCK_SIZE (1024*1024) // 1M
static unsigned char *ramblock_buf; // 分配得到的内存

static void do_ramblock_request(request_queue_t * q)
{
    // static int cnt = 0;
    // printk("do_ramblock_request: %d\n", ++cnt);

    // 用内存模拟块设备，在请求中拷贝数据
    struct request *req; // 取出请求之后要对其处理
    while ((req = elv_next_request(q)) != NULL) { // 用电梯调度算法取出下一个请求
        // 数据传输三要素:源 目地 长度
        unsigned long offset = req->sector << 9; // *512 源 一个扇区大小为512还原为Byte
        unsigned long len  = req->current_nr_sectors << 9; // *512 长度
        if (rq_data_dir(req) == READ) { // 读请求
            memcpy(req->buffer, ramblock_buf+offset, len); // 从磁盘中读取数据 内存模拟的磁盘
        } else {
            memcpy(ramblock_buf+offset, req->buffer, len); // 将buffer的数据写入磁盘 内存
        }

        end_request(req, 1);    // 已经对请求处理完成 1成功0失败
    }
}

static int ramblock_init(void)
{
    // 1.分配一个gendisk结构体
    ramblock_disk = alloc_disk(16); // 次设备号个数 分区个数 最多15个分区
    // 2.设置
    // 2.1分配/设置队列：提供读写功能
        // 需要提供处理队列的函数
    ramblock_queue = blk_init_queue(do_ramblock_request, &ramblock_lock);
    ramblock_disk->queue = ramblock_queue; 
    // 2.2设置其他属性：比如容量
    major = register_blkdev(0, "ramdisk"); // 注册，只有主设备号和name了 无fops
    ramblock_disk->major        = major;
    ramblock_disk->first_minor  = 0; // 15个次设备 从0开始
    sprintf(ramblock_disk->disk_name, "ramdisk");
    ramblock_disk->fops         = ramdisk_fops;
    set_capacity(ramblock_disk, RAMBLOCK_SIZE / 512); // 设置容量1M 以扇区为单位
                                // 内核文件系统中认为扇区是512Byte 
    
    // 3.硬件相关操作
    ramblock_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL); // 分配一块内存
    // 4.注册
    add_disk(ramblock_disk);
    
    return 0;
}
static void ramblock_exit(void)
{
    unregister_blkdev(major, "ramdisk");
    del_gendisk(ramblock_disk);
    put_disk(ramblock_disk);
    blk_cleanup_queue(ramblock_queue);
    kfree(ramblock_buf);
}
module_init(ramblock_init);
module_exit(ramblock_exit);
MODULE_LICENSE("GPL");