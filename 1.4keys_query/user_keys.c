#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int fd;
    int cnt = 0;
    unsigned char key_vals[4];
    
    if (argc != 1)
    {
        printf("No args required.\n\n");
        return 0;
    }

    fd = open("/dev/keys_node", O_RDWR);
    if (fd < 0)
    {
        printf("can't open '/dev/keys_node'.\n");
        return 0;
    }
    printf("Waiting press the keys, will print xxxx.\n\n");
    while(1)
    {
        read(fd, key_vals, sizeof(key_vals));
        if (!key_vals[0] || !key_vals[1] || !key_vals[2] || !key_vals[3])
            printf("%04d key pressed, the value is: %d %d %d %d\n\n", \
                    cnt++, key_vals[0], key_vals[1], key_vals[2], key_vals[3]);
    }
    return 0;
}
