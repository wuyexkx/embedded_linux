// #include <asm/io.h>
// #include <asm/irq.h>
// #include <asm/arch/regs-gpio.h>
// #include <asm/hardware.h>
#include <linux/proc_fs.h>
#include <linux/module.h>   // MODULE_LICENSE
// #include <linux/fs.h>
// #include <linux/kernel.h>
// #include <linux/init.h>
// #include <asm/uaccess.h>
// #include <linux/delay.h>


static struct proc_dir_entry *my_entry;
static const struct file_operations proc_mymsg_operations = {
	// .read		= kmsg_read,
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
MODULE_LICENSE("GPL");
