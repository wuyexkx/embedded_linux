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

// 仿真假装接收到 数据帧上报 数据链路层加上 MAC和帧校验序列(FCS frame check sequence)
//  没什么用，只是讲解 接收sk_buff上报
static void emulator_rx_packet(struct sk_buff *skb, struct net_device* dev)
{
// 参考LDDR3 很复杂 视频也没有自己写，一般用不到，主要是讲解怎么处理skb数据帧
//  修改 源MAC ip目的MAC ip  type等
    unsigned char *type;
    struct iphdr *ih;
    __be32 *saddr, *daddr, tmp;
    unsigned char tmp_dev_addr[ETH_ALEN];
    struct ethhdr *ethhdr;
    struct sk_buff *rx_skb;

    // 从硬件读出/保存数据
    //  对换源、目的的MAC
    ethhdr = (struct ethhdr*)skb->data;
    memcpy(tmp_dev_addr, ethhdr->h_dest, ETH_ALEN);
    memcpy(ethhdr->h_dest, ethhdr->h_source, ETH_ALEN);
    memcpy(ethhdr->h_source, tmp_dev_addr, ETH_ALEN);

    //  对换源、目的的ip
    ih = (struct iphdr*)(skb->data + sizeof(struct ethhdr));
    saddr = &ih->saddr;
    daddr = &ih->daddr;
    tmp = *saddr;
    *saddr = *daddr;
    *daddr = tmp;

    type = skb->data + sizeof(struct ethhdr) + sizeof(struct iphdr);
    *type = 0; // 修改类型，原来0x8表示ping，0表示replay

    ih->check = 0;
    ih->check = ip_fast_csum((unsigned char*)ih, ih->ihl);

    // 构造一个sk_buff
    rx_skb = dev_alloc_skb(skb->len + 2);
    skb_reserve(rx_skb, 2);
    memcpy(skb_put(rx_skb, skb->len), skb->data, skb->len);

    rx_skb->dev = dev;
    rx_skb->protocol = eth_type_trans(rx_skb, dev);
    rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
    dev->stats.rx_packets++;
    dev->stats.rx_bytes += skb->len;   

    // 提交sk_buff
    netif_rx(rx_skb);
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
    vnet_dev->hard_start_xmit = virtnet_sendpacket;

    //  ping通还需要设置两项
    vnet_dev->flags     |= IFF_NOARP;
    vnet_dev->features  |= NETIF_F_NO_CSUM; 

    //  设置MAC
    vnet_dev->dev_addr[0] = 0x08;
    vnet_dev->dev_addr[1] = 0x89;
    vnet_dev->dev_addr[2] = 0x89;
    vnet_dev->dev_addr[3] = 0x89;
    vnet_dev->dev_addr[4] = 0x89;
    vnet_dev->dev_addr[5] = 0x11;     

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