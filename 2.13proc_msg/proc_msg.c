#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/proc_fs.h>
#include <linux/module.h>   // MODULE_LICENSE
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/delay.h>


static struct proc_dir_entry *my_entry;
static char tmp_buf[1024];
/* 定义一个等待队列button_waitq，这个等待队列实际上是由中断驱动的，
    当中断发生时，会令挂接到这个等待队列的休眠进程唤醒 */
static DECLARE_WAIT_QUEUE_HEAD(log_waitq);

// -------------------------------------环形缓存区----------------------------------------------
// 环形缓冲区 log_buf
#define BUF_SIZE (1024)
#define DATA_TYPE char

/* 环形顺序缓存区
@BUF_SIZE    需要自定义两个宏 缓存区大小
@DATA_TYPE   存储数据的类型

@.head = 0,
@.tail = 0,
@.size = 0,
@.capacity = BUF_SIZE   */
typedef struct ring_buf ring_buf;
struct ring_buf{
    DATA_TYPE buf[BUF_SIZE];
    int head;
    int tail;
    int size;
    int capacity;
    int (*isempty)(ring_buf* rb);
    void (*put)(ring_buf* rb, DATA_TYPE c);    
    int (*gets)(ring_buf* rb, DATA_TYPE* dst,  int len);
};
/* 往环形缓存区存入数据
@ring_buf* rb 需要处理的环形缓存区 通常为THIS
@DATA_TYPE c  需要存入的数据    */ 
static void ring_buf_put(ring_buf* rb, DATA_TYPE c)
{
    rb->tail = (rb->tail + 1) % rb->capacity;
    if (rb->head == rb->tail) {  // 满了就覆盖
        rb->head = (rb->head + 1) % rb->capacity;
        rb->size = rb->capacity;
    }
    else rb->size++;               
    rb->buf[rb->tail] = c;
}
/* 检查环形缓存区是否为空
@ring_buf* rb 需要处理的环形缓存区 通常为THIS    */ 
static int ring_buf_isempty(ring_buf* rb)
{ return rb->head == rb->tail; } 
/* 从环形缓存区中获取数据
@ring_buf* rb   需要处理的环形缓存区 通常为THIS 
@DATA_TYPE* dst 存入目标
@int len        获取的数据的个数    
return: 成功复制到dst的数据个数 */
static int ring_buf_gets(ring_buf* rb, DATA_TYPE* dst, int len)
{
    // 实际能复制的长度
    int cp_len = len > rb->size ? rb->size : len;    
    // 缓存区为空
    if (rb->isempty(rb)) return 0;

    // 将数据复制到dst，检查是否wrap
    if (rb->tail > rb->head)        // not wrap
        memcpy(dst, rb->buf + 1, cp_len * sizeof(DATA_TYPE));
    else {                          // wrap
        int len_back;              // 后部分的长度
        len_back = rb->capacity - rb->head;
        if (cp_len <= len_back)     // 需要的数据长度 <= 后半部的长度 直接复制需要的长度即可
            memcpy(dst, rb->buf + rb->head, cp_len * sizeof(DATA_TYPE));
        else {                      // 需要的数据长度 > 后半部的长度 分两部分复制
            memcpy(dst, rb->buf + rb->head, len_back * sizeof(DATA_TYPE));           // 后半部
            memcpy(dst + len_back, rb->buf, (cp_len - len_back) * sizeof(DATA_TYPE));// 前半部
        }
    }
    return cp_len;
}

// 定义并初始化一个缓存区
static ring_buf log_buf = {
    .head = 0,
    .tail = 0,
    .size = 0,
    .capacity = BUF_SIZE,
    .isempty = ring_buf_isempty,
    .put = ring_buf_put,
    .gets = ring_buf_gets,
};
// -------------------------------------------------------------------------------------------

// 巧妙用vsnprintf将日志写到临时缓存区
static int myprintk(const char *fmt, ...)
{
    va_list args; // typedef char *va_list;
    int len, i;
    va_start(args, fmt);    // ? 拆解可变参数？
    len = vsnprintf(tmp_buf, INT_MAX, fmt, args);
    va_end(args);

    // 将临时缓存区数据放入环形缓存区中
    for (i=0; i<len; ++i) 
        log_buf.put(&log_buf, tmp_buf[i]);
    // 当缓存区有数据后，唤醒等待log_waitq的进程
    wake_up_interruptible(&log_waitq);

    return len;
}

// 参考fs/proc/kmsg.c的kmsg_read,
static ssize_t mymsg_read(struct file *file, char __user *buff, size_t count, loff_t *ppos)
{
    static int read_flag = 1;

    int err, i=0, cp_len;
    // 非阻塞方式打开 并且 buf为空 返回错误咯
    if ((file->f_flags & O_NONBLOCK) && log_buf.isempty(&log_buf))
        return -EAGAIN;

    // 阻塞方式打开文件时 休眠直到buf不为空
    // 休眠时放入队列log_wait，唤醒时在队列里找，在写数据后唤醒
    err = wait_event_interruptible(log_waitq, !log_buf.isempty(&log_buf));
    // 被唤醒之后做点事，copy_to_user
    // 方法一 类似视频==================================================
    // cp_len = log_buf.gets(&log_buf, tmp_buf, count);   // 先从log_buf中取出到tmp_buf
    // while (!err && i < cp_len) {                       // 然后搬运到用户buff
    //     err = __put_user(tmp_buf[i], buff);
    //     buff++;
    //     i++;
    // }
    // // 没有出错返回i，出错返回错误值
    // if (!err)  
    //     err = i;

    // // cat /proc/mymsg 时打印不停 因为返回永远是有长度的
    // //  临时解决 flag 0 1
    // read_flag = read_flag == 1 ? 0 : 1; // 改变值
    // if (read_flag == 0) return 0;       // 首次读取返回读取长度
    // return err;  

    // 方法二 不用copy_to_user直接memcpy ================================
    // cat /proc/mymsg 时打印不停 因为返回永远是有长度的
    //  临时解决 flag 0 1
    read_flag = read_flag == 1 ? 0 : 1; // 改变值
    if (read_flag == 1) return 0;       // 首次读取返回读取长度
    return log_buf.gets(&log_buf, buff, count);
}
static const struct file_operations proc_mymsg_operations = {
	.read		= mymsg_read,
	// .poll		= kmsg_poll,
	// .open		= kmsg_open,
	// .release	= kmsg_release,
};

static int proc_msg_init(void)
{
    my_entry = create_proc_entry("mymsg", S_IRUSR, &proc_root);
    if (my_entry)
        my_entry->proc_fops = &proc_mymsg_operations;
    
    return 0;
}
static void proc_msg_exit(void)
{
    remove_proc_entry("mymsg", &proc_root);
}
module_init(proc_msg_init);
module_exit(proc_msg_exit);
EXPORT_SYMBOL(myprintk);    // 导出
MODULE_LICENSE("GPL");
// EXPORT_SYMBOL是Linux内核中一个常见的工具，
// 其作用是讲一个”Symbol”（函数或者变量）导出到内核空间，使得内核的所有代码都可以使用。