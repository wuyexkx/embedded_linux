KERN_DIR := /home/wuyexkx/Desktop/韦东山/system/linux-2.6.22.6
PWD := $(shell pwd)

obj-m := proc_msg.o
obj-m += led_myprintk.o

all:
	make -C $(KERN_DIR) M=$(PWD) modules CROSS_COMPILE=arm-linux- ARCH=arm 
clean:
	make -C $(KERN_DIR) M=$(PWD) clean

# 编译复制清空
# rm /nfs/fs_root/tzb/* ./*.ko ./*.o ./led_user -r
# make && arm-linux-gcc -o led_user led_user.c
# cp ./*.ko ./led_user /nfs/fs_root/tzb/

# 加载模块 测试用户
# rmmod led_myprintk
# rmmod proc_msg
# insmod proc_msg.ko
# insmod led_myprintk.ko
# ./led_user led0 on
# ./led_user led0 off
# cat /proc/mymsg
