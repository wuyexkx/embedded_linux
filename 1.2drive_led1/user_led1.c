#include <fcntl.h>
#include <stdio.h>

// main传入2个参数 user_led1 on/off
int main(int argc, char **argv)
{
    int fd;
    int val = 1;
    // 打开 mdev根据驱动程序中的class_device_create自动创建的 设备节点
    fd = open("/dev/led_on_off1", O_RDWR);
    if(fd < 0)
        printf("can't open '/dev/led_on_off1'\n");
    
    // main参数个数判断
    if(argc != 2) 
    {
        printf("Usage:\n");
        printf("\t%s <on|off>\n", argv[0]);
        return 0;
    }
    // 第二参数判断
    if(0 == strcmp(argv[1], "on"))      // 开灯参数
    {
        val = 1;
    }
    else if(0 == strcmp(argv[1], "off"))// 关灯参数
    {
        val  = 0;
    }
    // 根据第二参数write
    write(fd, &val, 4);

    return 0;
}
