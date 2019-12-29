#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd, ret;
    unsigned char key_val;

    fd = open("/dev/keys_all_node", O_RDWR);           // 默认阻塞方式
    // fd = open("/dev/keys_all_node", O_RDWR | O_NONBLOCK); // 非阻塞方式
    if(fd < 0)
    {
        printf("can't open '/dev/keys_all_node'.\n");
        return -1;
    }
    while(1)
    {
        ret = read(fd, &key_val, 1);
        printf("signal read key_val: 0x%x, ret: %d.\n", key_val, ret);
        // sleep(3); // 单位s
    }
    return 0;
}
