KERN_DIR := /home/wuyexkx/Desktop/韦东山/system/linux-2.6.22.6

PWD := $(shell pwd)		# 执行pwd命令并把结果赋给PWD

obj-m := demo_chrdev.o  # .ko的生成依赖于.o，.o默认依赖.c

all:
	make -C $(KERN_DIR) M=$(PWD) modules CROSS_COMPILE=arm-linux- ARCH=arm # modules为编译目标
clean:
	make -C $(KERN_DIR) M=$(PWD) clean