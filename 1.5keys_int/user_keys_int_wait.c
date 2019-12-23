#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int fd;
    int cnt = 0;
    unsigned char key_val;
    
    if (argc != 1)
    {
        printf("No args required.\n\n");
        return 0;
    }

    fd = open("/dev/k_int_w_node", O_RDWR);
    if (fd < 0)
    {
        printf("can't open '/dev/k_int_w_node'.\n");
        return 0;
    }
    printf("Waiting press the keys, will print xxxx.\n\n");
    while(1)
    {
        // read之后进入内核态的驱动程序里，如果没有按键中断，休眠;
        //  有按键中断，进入ISR得到按键值，在read中继续执行，将按键值传递到用户空间
        read(fd, &key_val, 1);
        printf("key_val: 0x%x\n", key_val); // 输出16进制
    }
    return 0;
}
