KERN_DIR := /home/wuyexkx/Desktop/韦东山/system/linux-2.6.22.6
PWD := $(shell pwd)

obj-m := keys_async.o

all:
	make -C $(KERN_DIR) M=$(PWD) modules CROSS_COMPILE=arm-linux- ARCH=arm
clean:
	make -C $(KERN_DIR) M=$(PWD) clean

