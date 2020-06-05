#include <fcntl.h>
#include <stdio.h>
#include <string.h>

void print_usage(const char *argv0)
{
    printf("Usage:\n");
    printf("\t%s led<x> <on|off>\t[x=0~3]\n", argv0);
    printf("Example:\n");
    printf("\t%s led0 on\n", argv0);
    printf("\t%s led0 off\n\n", argv0);
}

// main传入3个参数 user_led2 led0 on/off
int main(int argc, char **argv)
{
    int fd;
    int is_off = 1;

    // main参数个数判断
    if (argc != 3)
    {
        print_usage(argv[0]);
        return 0;
    }  

    // 保存设备节点名称
    char device_name[10] = "/dev/";
    // 根据用户输入得到完整设备节点名称 "/dev/" + "led0" = "/dev/led0"
    strcat(device_name, argv[1]);
      
    // 打开 mdev根据驱动程序中的class_device_create自动创建的 设备节点
    fd = open(device_name, O_RDWR);
    if(fd < 0)
    {
        printf("can't open %s.\n", argv[1]);
        return 0;
    }    
    // 第三参数判断
    if(0 == strcmp(argv[2], "on"))      // 开灯参数
    {
        is_off = 0;
        // 根据第三参数write
        write(fd, &is_off, 1);
    }
    else if(0 == strcmp(argv[2], "off"))// 关灯参数
    {
        is_off = 1;
        // 根据第三参数write
        write(fd, &is_off, 1);
    }
    else                                // 第三参数错误
    {
        printf("Unknown args '%s'.\n\n", argv[2]);
    }
    
    return 0;
}
