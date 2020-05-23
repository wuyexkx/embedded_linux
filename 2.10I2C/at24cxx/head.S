@******************************************************************************
@ File: head.S
@ 功能: 设置SDRAM，将程序复制到SDRAM，然后跳到SDRAM继续执行
@******************************************************************************       
   
@ .text 部分是处理器开始执行代码的地方,指定了后续编译出来的内容放在代码段【可执行】,是arm-gcc编译器的关键词。
@ .global关键字用来让一个符号对链接器可见，可以供其他链接对象模块使用;告诉编译器后续跟的是一个全局可见的名字【可能是变量，也可以是函数名】
@ linux寻找这个 _start 标签作为程序的默认进入点。

.extern     main
.text 
.global _start 
_start:
@******************************************************************************       
@ 中断向量，本程序中，除Reset和HandleIRQ外，其它异常都没有使用
@******************************************************************************       
    b   Reset

@ 0x04: 未定义指令中止模式的向量地址
HandleUndef:
    b   HandleUndef 
 
@ 0x08: 管理模式的向量地址，通过SWI指令进入此模式
HandleSWI:
    b   HandleSWI

@ 0x0c: 指令预取终止导致的异常的向量地址
HandlePrefetchAbort:
    b   HandlePrefetchAbort

@ 0x10: 数据访问终止导致的异常的向量地址
HandleDataAbort:
    b   HandleDataAbort

@ 0x14: 保留
HandleNotUsed:
    b   HandleNotUsed

@ 0x18: 中断模式的向量地址
    b   HandleIRQ

@ 0x1c: 快中断模式的向量地址
HandleFIQ:
    b   HandleFIQ

Reset:                  
    ldr sp, =4096           @ 设置栈指针，以下都是C函数，调用前需要设好栈  设置栈地址在4k RAM的最高处，sp=4096;
    bl  disable_watch_dog   @ 关闭WATCHDOG，否则CPU会不断重启
    bl  clock_init          @ 设置MPLL，改变FCLK、HCLK、PCLK
    bl  memsetup            @ 设置存储控制器以使用SDRAM
    bl  nand_init           @ 初始化NAND Flash
    
@ 传递参数：通过寄存器r0~r3来向子函数传递参数，当参数个数多于4个时，使用栈来传递参数。 
@           当子函数运行时，根据自身参数个数自动从R0~R3或者栈中读取参数。 
@ 返回值：子函数通过R0寄存器将返回值传递给父函数。存入R0，父函数读取R0获得返回值。 
                            @ 复制代码到SDRAM中
    ldr r0, =0x30000000     @ 1. 目标地址 = 0x30000000，这是SDRAM的起始地址 
    mov r1, #0              @ 2. 源地址   = 0，从nand的0地址开始拷贝
    ldr r2, =__bss_start
    sub r2, r2, r0          @ 3. 复制长度 r2 = r2 - r0  (__bss_start - 0x30000000) 
    bl  CopyCode2SDRAM      @ 调用C函数CopyCode2SDRAM   copy2ram前面的代码必须是位置无关码
    
    bl  clean_bss           @ 清除bss段，未初始化或初值为0的全局/静态变量保存在bss段

    msr cpsr_c, #0xd2       @ 进入中断模式
    ldr sp, =0x31000000     @ 设置中断模式栈指针

    msr cpsr_c, #0xdf       @ 进入系统模式
    ldr sp, =0x34000000     @ 设置系统模式栈指针，

    ldr lr, =ret_initirq    @ 设置返回地址    
    ldr pc, =init_irq       @ 调用中断初始化函数
ret_initirq:
    msr cpsr_c, #0x5f       @ 设置I-bit=0，开IRQ中断

    ldr lr, =halt_loop      @ 设置返回地址
    ldr pc, =main           @ 调用main函数
halt_loop:
    b   halt_loop

HandleIRQ:
    sub lr, lr, #4                  @ 计算返回地址
    stmdb   sp!,    { r0-r12,lr }   @ 保存使用到的寄存器
                                    @ 注意，此时的sp是中断模式的sp
                                    @ 初始值是上面设置的4096
    
    ldr lr, =int_return             @ 设置调用IRQ_Handle函数后的返回地址  
    ldr pc, =IRQ_Handle             @ 调用中断分发函数，在interrupt.c中
int_return:
    ldmia   sp!,    { r0-r12,pc }^  @ 中断返回, ^表示将spsr的值复制到cpsr
    
