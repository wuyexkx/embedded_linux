#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

static struct i2c_driver at24cxx_driver;

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x50, I2C_CLIENT_END }; // 地址只包含前7位 0x50，设备地址1010 0000

static struct i2c_client_address_data addr_data = {
    .normal_i2c = normal_addr, // 要发出S信号和设备地址并得到ACK才能确定设备存在 
    .probe      = ignore,
    .ignore     = ignore,
};
// 那个function
static int at24cxx_detect(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *client;
    // 构造i2c_client adapter指向左边，driver指向右边，地址就是发现的那个设备地址
    //  收发数据就可以用i2c_client完成
	client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);    
	client->addr    = address;
	client->adapter = adapter;
	client->driver  = &at24cxx_driver;
    strlcpy(client->name, "at24cxx");

    i2c_attach_client(client);

    printk("at24cxx_detect\n");
    return 0;
}
static int at24cxx_attach(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, at24cxx_detect);
}

static int at24cxx_detach(struct i2c_client *client)
{
    printk("at24cxx_detach\n");
    i2c_detach_client(client); // 对应i2c_attach_client
    kfree(i2c_get_clientdata(client));
    return 0;
}

static struct i2c_driver at24cxx_driver = {
	.driver = {
		.name	= "at24cxx",
	},
	.attach_adapter = at24cxx_attach,
	.detach_client  = at24cxx_detach,
};

static int at24xxx_init(void)
{
    // 1. 分配一个i2c_driver结构体，这里直接前面定义这个结构体
    i2c_add_driver(&at24cxx_driver);
    // 2. 设置
    // 	attach_adapter直接调用i2c_probe(adap, 设备地址，发现这个设备后要调用的函数)；
    // 	detach_client卸载这个驱动后，如果之前发能够支持的设备，则调用它来清理；
    // 3.注册 i2c_add_driver  
    return 0;    
}
static void at24xxx_exit(void)
{
    i2c_del_driver(&at24cxx_driver);
}

module_init(at24xxx_init);
module_exit(at24xxx_exit);
MODULE_LICENSE("GPL");