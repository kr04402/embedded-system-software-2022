#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <asm/gpio.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/cdev.h>

// physical address
#define IOM_FND_ADDRESS 0x08000004

static struct struct_mydata {
	struct timer_list timer;
	int count;
};

static int inter_major=242, inter_minor=0;
static int result;
static dev_t inter_dev;
static struct cdev inter_cdev;

static int inter_open(struct inode *, struct file *);
static int inter_release(struct inode *, struct file *);
static int inter_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

irqreturn_t inter_handler1(int irq, void* dev_id, struct pt_regs* reg);     /* HOME */
irqreturn_t inter_handler2(int irq, void* dev_id, struct pt_regs* reg);     /* BACK */
irqreturn_t inter_handler3(int irq, void* dev_id, struct pt_regs* reg);     /* VOL+ */
irqreturn_t inter_handler4(int irq, void* dev_id, struct pt_regs* reg);     /* VOL- */

void kernel_timer_blink(unsigned long timeout);
void fnd_write(int val);
void timer_stop(void);
void short_do_tasklet(unsigned long);

// variables added
static int inter_usage=0;
static unsigned char *iom_fpga_fnd_addr;
struct struct_mydata mydata,stop_timer;
int timer_usage=0;        /* whether timer is running */
int temp_time;            /* leftover time when paused */
int start_time;           /* timer start time */
int total_time;           /* fnd printed time */

wait_queue_head_t wq_write;
DECLARE_WAIT_QUEUE_HEAD(wq_write);
DECLARE_TASKLET(short_tasklet, short_do_tasklet, 0);

static struct file_operations inter_fops =
{
	.open = inter_open,
	.write = inter_write,
	.release = inter_release,
};

// HOME
irqreturn_t inter_handler1(int irq, void* dev_id, struct pt_regs* reg) {
	printk(KERN_ALERT "Home Button!!!\n");
    if(timer_usage==1) /* already running */
    {
        printk(KERN_ALERT "timer already running\n");
        return IRQ_HANDLED;
    }
    timer_usage=1;

    start_time=get_jiffies_64();
	del_timer(&mydata.timer);
    // needs to start timer after 0.x seconds if paused
    if(!temp_time) temp_time=1*HZ;
	printk("temptime checking:%d\n",temp_time);

    mydata.timer.expires = get_jiffies_64() + temp_time;    
	mydata.timer.data = (unsigned long)&mydata;
	mydata.timer.function = kernel_timer_blink;
    add_timer(&mydata.timer);
    temp_time=0;

	return IRQ_HANDLED;
}
// BACK
irqreturn_t inter_handler2(int irq, void* dev_id, struct pt_regs* reg) {
        printk(KERN_ALERT "BACK Button!!!\n");
        if(!timer_usage)  
        {
            printk(KERN_ALERT "timer is not running. cannot pause\n");
            return IRQ_HANDLED;
        }
        timer_usage=0;  /* pause it(not using) */
        // calculate temp_time
        // printk("HZ test: %d\n",HZ);
        temp_time=(get_jiffies_64()-start_time);
		temp_time=1*HZ-temp_time;
        printk("temptime:0.%ds left\n",temp_time);
        return IRQ_HANDLED;
}
// VOL+
irqreturn_t inter_handler3(int irq, void* dev_id,struct pt_regs* reg) {
        printk(KERN_ALERT "VOL+ Button!!!\n");
        // doesn't matter whether the timer is paused or running
        fnd_write(0);
        temp_time=0; total_time=0; timer_usage=0;
		del_timer(&mydata.timer);
		// timer restart needed? ==> press HOME again
        return IRQ_HANDLED;
}
// VOL-
irqreturn_t inter_handler4(int irq, void* dev_id, struct pt_regs* reg) {
        if(!gpio_get_value(IMX_GPIO_NR(5,14)))
        {
            printk(KERN_ALERT "VOL- Button!!!\n");
            // 3sec timer
            stop_timer.timer.expires = get_jiffies_64() + 3*HZ;  /* should be pressing for 3 seconds */  
	        stop_timer.timer.data = (unsigned long)&stop_timer;
	        stop_timer.timer.function = timer_stop;
            add_timer(&stop_timer.timer);
            return IRQ_HANDLED;
        }
		// when released before 3seconds
		del_timer(&stop_timer.timer);
        return IRQ_HANDLED;
}

void kernel_timer_blink(unsigned long timeout)
{
    // if paused, should not update fnd
    if(!timer_usage) return;
	start_time=get_jiffies_64();

    (total_time)++;
    fnd_write(total_time);

    mydata.timer.expires = get_jiffies_64() + (1 * HZ);     /* interval: every second */
	mydata.timer.data = (unsigned long)&mydata;
	mydata.timer.function = kernel_timer_blink;

	add_timer(&mydata.timer);
}
void fnd_write(int val)
{
    unsigned char value[4];
	unsigned short int value_short = 0;
    
    int min=val/60; int sec=val%60;
    value[0]=min/10; value[1]=min%10; value[2]=sec/10; value[3]=sec%10;
    // outw: 16bit unit
    value_short = value[0] << 12 | value[1] << 8 |value[2] << 4 |value[3];
    outw(value_short,(unsigned int)iom_fpga_fnd_addr);	
}
void timer_stop(void )
{
    // timer delete
    del_timer_sync(&mydata.timer);
    timer_usage=0; total_time=0;
    fnd_write(0);
    printk(KERN_ALERT "Ending Stopwatch\n");
    // wakeup -> bottom half interrupt
    tasklet_schedule(&short_tasklet);   
}

// bottom half interrupt
//-----------------------------------------------------------------------
void short_do_tasklet(unsigned long unused)
{
    __wake_up(&wq_write, 1, 1, NULL);
    printk(KERN_ALERT "Wakeup\n");
}
//-----------------------------------------------------------------------

static int inter_open(struct inode *minode, struct file *mfile){
	int ret;
	int irq;
    if(inter_usage!=0) 
        return -EBUSY;
    inter_usage=1;

	printk(KERN_ALERT "Open Module\n");

	// int1		home
	gpio_direction_input(IMX_GPIO_NR(1,11));
	irq = gpio_to_irq(IMX_GPIO_NR(1,11));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler1, IRQF_TRIGGER_FALLING, "home", 0);

	// int2		back
	gpio_direction_input(IMX_GPIO_NR(1,12));
	irq = gpio_to_irq(IMX_GPIO_NR(1,12));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler2, IRQF_TRIGGER_FALLING, "back", 0);

	// int3		vol+
	gpio_direction_input(IMX_GPIO_NR(2,15));
	irq = gpio_to_irq(IMX_GPIO_NR(2,15));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler3, IRQF_TRIGGER_FALLING, "volup", 0);

	// int4		vol-
	gpio_direction_input(IMX_GPIO_NR(5,14));
	irq = gpio_to_irq(IMX_GPIO_NR(5,14));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler4, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "voldown", 0);

	temp_time=0; total_time=0; timer_usage=0;

	return 0;
}

static int inter_release(struct inode *minode, struct file *mfile){
    inter_usage=0; 
	free_irq(gpio_to_irq(IMX_GPIO_NR(1, 11)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(1, 12)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(2, 15)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(5, 14)), NULL);
	
	printk(KERN_ALERT "Release Module\n");
	return 0;
}

static int inter_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos ){
    printk("sleep on\n");
    interruptible_sleep_on(&wq_write);
	printk("write\n");
	return 0;
}

static int inter_register_cdev(void)
{
	int error;
	if(inter_major) {
		inter_dev = MKDEV(inter_major, inter_minor);
		error = register_chrdev_region(inter_dev,1,"inter");
	}else{
		error = alloc_chrdev_region(&inter_dev,inter_minor,1,"inter");
		inter_major = MAJOR(inter_dev);
	}
	if(error<0) {
		printk(KERN_WARNING "inter: can't get major %d\n", inter_major);
		return result;
	}
	
	cdev_init(&inter_cdev, &inter_fops);
	inter_cdev.owner = THIS_MODULE;
	inter_cdev.ops = &inter_fops;
	error = cdev_add(&inter_cdev, inter_dev, 1);
	if(error)
	{
		printk(KERN_NOTICE "inter Register Error %d\n", error);
	}
	return 0;
}

static int __init inter_init(void) {
	int result;
	if((result = inter_register_cdev()) < 0 )
		return result;
    // timer initiate
    init_timer(&mydata.timer);
    init_timer(&stop_timer.timer);
	// device
	iom_fpga_fnd_addr = (unsigned char*)ioremap(IOM_FND_ADDRESS, 0x4);

	printk(KERN_ALERT "Init Module Success \n");
	printk(KERN_ALERT "Device : /dev/stopwatch, Major Num : 242 \n");
	return 0;
}

static void __exit inter_exit(void) {
	cdev_del(&inter_cdev);
	unregister_chrdev_region(inter_dev, 1);
    // timer delete
    del_timer_sync(&mydata.timer);
    del_timer_sync(&stop_timer.timer);
	// device
	iounmap(iom_fpga_fnd_addr);

	printk(KERN_ALERT "Remove Module Success \n");
}

module_init(inter_init);
module_exit(inter_exit);
MODULE_LICENSE("GPL");
