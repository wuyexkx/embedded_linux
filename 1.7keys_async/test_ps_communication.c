#include <stdio.h>
#include <signal.h>


void signal_func(int sign)
{
    static int cnt = 0;
    printf("signal: %d, %d times.\n", signal, ++cnt);
}

// kill -USR1 PID == kill -10 PID
// kill -9 PID // 杀掉进程

int main(int argc, char **argv)
{
    // 信号处理函数, 
    signal(SIGUSR1, signal_func);
    while(1)
    {
        sleep(1000);
    }

    return 0;
}
