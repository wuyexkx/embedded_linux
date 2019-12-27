#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

int fd;
void signal_func(int sign)
{
    unsigned char key_val;
    read(fd, &key_val, 1);
    printf("signal read key_val: 0x%x.\n", key_val);
}

int main(int argc, char **argv)
{
    int oflag;

    // 1. 应用程序注册信号处理函数
    // 2. 谁发，在驱动程序中IRQ中kill_fasync发
    //      驱动代码的kill_fasync (&keys_async, SIGIO, POLL_IN);通知用户signal函数
    signal(SIGIO, signal_func);
    fd = open("/dev/k_async_node", O_RDWR);
    if(fd < 0)
    {
        printf("can't open '/dev/k_async_node'.\n");
    }
    // 3. 发给谁，应用程序通过这里告诉驱动程序 发给这个pid
    fcntl(fd, F_SETOWN, getpid());
    // 改变oflag之后驱动程序中的fops.fasync才会被调用，
    //      fasync被调用之后才用helper去初始化fasync_struct这个结构体
    //      那个结构体被初始化后，驱动才知道向谁发信号
    oflag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, oflag | FASYNC);

    while(1)
    {
        sleep(1000);
    }

    return 0;
}
