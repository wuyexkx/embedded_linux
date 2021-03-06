#include <linux/module.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>


// 参考 drivers\net\cs89x0.c

static struct net_device* vnet_dev;


static void xxxxxx(struct sk_buff *skb, struct net_device* dev)
{

}

static int virtnet_sendpacket(struct sk_buff *skb, struct net_device *dev)
{
    static int cnt = 0;
    printk("virtnet_sendpacket cnt = %d\n", ++cnt);

    // 对于真实的网卡，应该把skb数据通过网卡发送出去
    //  数据写入发送完成后会产生中断再唤醒该队列
    netif_stop_queue(vnet_dev);  // 停止该网卡队列
    // 。。。。                   // 把skb的数据写入网卡
    //  自己构造sk_buff，假装接收到数据帧，然后上报
    emulator_rx_packet(skb, vnet_dev);     
    dev_free_skb(skb);           // 释放skb
    netif_wake_queue(vnet_dev);  // 数据全部发送完成后会唤醒网卡队列（应该在中断函数中完成）
    // 更新统计信息
    vnet_dev->stats.tx_packets++;
    vnet_dev->stats.tx_bytes += skb->len;

    return 0;
}

static int virt_net_init(void)
{
    // 1.分配net_device
    //  私有数据设置为0 
    vnet_dev = alloc_netdev(0, "vnet%d", ether_setup);
    // 2.设置
    vnet_dev->hard_start_xmit = xxxxxx;

    // 3.注册
    register_netdev(vnet_dev);
    return 0;
}
static void virt_net_exit(void)
{
    unregister_netdev(vnet_dev);
    free_netdev(vnet_dev);
}
module_init(virt_net_init);
module_exit(virt_net_exit);
MODULE_LICENSE("GPL");