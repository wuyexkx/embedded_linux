#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h> 

// i2c_test r addr
// i2c_test w addr val

void print_usage(char *file)
{
    printf("%s r addr\n", file);
    printf("%s w addr val\n", file);
}

int main(int argc, char **argv)
{
    if ((argc != 3) && (argc != 4)) {
        print_usage(argv[0]);
        return -1;
    }

    int fd;
    fd = open("/dev/at24cxx", O_RDWR);
    if (fd < 0) {
        printf("can't open /dev/at24cxx\n");
        return -1;
    }

    unsigned char buff[2];
    // 读写操作
    if (strcmp(argv[1], "r") == 0) {
        buff[0] = strtoul(argv[2], NULL, 0); // 把字符转化成数字
        read(fd, buff, 1);
        printf("read data: %c, %d, 0x%2x\n", buff[0], buff[0], buff[0]);
    }
    else if (strcmp(argv[1], "w") == 0) {
        buff[0] = strtoul(argv[2], NULL, 0);
        buff[1] = strtoul(argv[3], NULL, 0);
        write(fd, buff, 2);
    }
    else {
        print_usage(argv[0]);
        return -1;
    }
    return 0;
}
