#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MEM_CPY_NO_DMA  0
#define MEM_CPY_DMA     1

void print_usage(char **argv)
{
    printf("Usage:\n");
    printf("%s <nodma | dma>\n", argv[0]);
}

int main(int argc, char **argv)
{
    if (argc !=2)
    {
        print_usage(argv);
        return -1;
    }    
    
    int fd = open("/dev/s3c_dma", O_RDWR);
    if (fd < 0)
    {
        printf("can't open /dev/s3c_dma.\n");
        return -1;
    }
    
    if (strcmp(argv[1], "nodma") == 0)
    {
        while (1)
        {
            ioctl(fd, MEM_CPY_DMA);
        }
    }
    else if (strcmp(argv[1], "dma") == 0)
    {
        while (1)
        {
            ioctl(fd, MEM_CPY_NO_DMA);
        }
    }
    else 
    {
        print_usage(argv);
        return -1;
    }

    return 0;
}
