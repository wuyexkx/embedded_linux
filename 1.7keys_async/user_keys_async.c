#include <fcntl.h>
#include <stdio.h>
#include <poll.h>

int main(int argc, char **argv)
{
    int fd;
    int cnt = 0;
    unsigned char key_val;
    struct pollfd fds[1]; // 同时查询多个文件（驱动程序）
    int ret;
    
    if (argc != 1)
    {
        printf("No args required.\n\n");
        return 0;
    }

    fd = open("/dev/k_poll_node", O_RDWR);
    if (fd < 0)
    {
        printf("can't open '/dev/k_poll_node'.\n");
        return 0;
    }
    printf("Waiting press the keys, will print xxxx.\n\n");

    fds[0].fd     = fd;     // 查询打开的文件
    fds[0].events = POLLIN; // 有数据等待读取
    while(1)
    {   
        // 查询一个文件，超时时间
        //  在应用程序中用select和poll最终都是在内核中调用poll，效果一样
        ret = poll(fds, 1, 2000); 
        if (ret == 0) // 超时
        {
            printf("timeout 2000ms\n");
        }
        else          // 有中断 
        {
            // read之后进入内核态的驱动程序里，如果没有按键中断，休眠2000ms退出，有中断则poll返回非0
            //  有按键中断，进入ISR得到按键值，在read中继续执行，将按键值传递到用户空间
            read(fd, &key_val, 1);
            printf("key_val: 0x%x\n", key_val); // 输出16进制
        }
    }
    return 0;
}
