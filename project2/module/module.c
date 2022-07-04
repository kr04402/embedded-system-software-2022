#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include "./fpga_dot_font.h"
#include <asm/uaccess.h>

// device driver settings
#define DEV_MAJOR 242 
#define DEV_MINOR 0
#define DEV_NAME "dev_driver"

// physical address
#define IOM_FND_ADDRESS 0x08000004
#define IOM_LED_ADDRESS 0x08000016
#define IOM_FPGA_DOT_ADDRESS 0x08000210 
#define IOM_FPGA_TEXT_LCD_ADDRESS 0x08000090

// IOCTL
#define SET_OPTION _IOW(DEV_MAJOR,1,int*)
#define COMMAND _IOW(DEV_MAJOR,2,int*)

static int kernel_timer_usage = 0;
// using devices
static unsigned char *iom_fpga_fnd_addr;
static unsigned char *iom_fpga_led_addr;
static unsigned char *iom_fpga_dot_addr;
static unsigned char *iom_fpga_text_lcd_addr;

int kernel_timer_open(struct inode *, struct file *);
int kernel_timer_release(struct inode *, struct file *);
long kernel_timer_ioctl(struct file *, unsigned int, unsigned long);

void fnd_write(int ,int );
void led_write(int );
void dot_write(int );
void lcd_write(int ,int );

static struct file_operations kernel_timer_fops =
{ .open = kernel_timer_open, .unlocked_ioctl = kernel_timer_ioctl,
	.release = kernel_timer_release };

static struct struct_mydata {
	struct timer_list timer;
	int count;
};

struct struct_mydata mydata;
int interval,position,value,original_value;
char student_num[9]="20161564";
char student_name[20]="WOOCHEOLKWAK";
int first_shift=0,second_shift=0,first_flag=0,second_flag=0;

int kernel_timer_release(struct inode *minode, struct file *mfile) {
	//printk("kernel_timer_release\n");
	kernel_timer_usage = 0;
	return 0;
}

int kernel_timer_open(struct inode *minode, struct file *mfile) {
	//printk("kernel_timer_open\n");
	if (kernel_timer_usage != 0) {
		return -EBUSY;
	}
	kernel_timer_usage = 1;
	return 0;
}

static void kernel_timer_blink(unsigned long timeout) {
	struct struct_mydata *p_data = (struct struct_mydata*)timeout;
    int i;
    unsigned char temp[33];
    unsigned short int _s_value = 0;

	// printk("kernel_timer_blink %d\n", p_data->count);
    if(p_data->count<=0)
    {
        // initialize and exit
        fnd_write(0,0);
        led_write(0);
        for(i=0;i<10;i++)
        {
		    outw(0,(unsigned int)iom_fpga_dot_addr+i*2);
        }
	    temp[32]=0;    
        for(i=0;i<32;i++) temp[i]=' '; 
	    for(i=0;i<32;i++)
        {
            _s_value = ((temp[i] & 0xFF) << 8) | (temp[i + 1] & 0xFF);
		    outw(_s_value,(unsigned int)iom_fpga_text_lcd_addr+i);
            i++;
        }
        return;
    }
    // update
    value++; value%=8;
    if(value==0) value=8;
    if(value==original_value)
    {
        position=(position+1)%4;
    }
    if(first_flag==0)
    {
        first_shift++;
        if(first_shift==9)
        {
            first_shift=7;
            first_flag=1;
        }
    }
    else
    {
        first_shift--;
        if(first_shift==-1)
        {
            first_shift=1;
            first_flag=0;
        }
    }
    if(second_flag==0)
    {
        second_shift++;
        if(second_shift==5)
        {
            second_shift=3;
            second_flag=1;
        }
    }
    else 
    {
        second_shift--;
        if(second_shift==-1)
        {
            second_shift=1;
            second_flag=0;
        }
    }

    fnd_write(value,position);
    led_write(value);
    dot_write(value);
    lcd_write(first_shift,second_shift);

    (p_data->count)--;

	mydata.timer.expires = get_jiffies_64() + (interval * HZ/10);
	mydata.timer.data = (unsigned long)&mydata;
	mydata.timer.function = kernel_timer_blink;

	add_timer(&mydata.timer);
}

long kernel_timer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int init,div;
    int data[2];
    // position=0; value=0; first_shift=0; second_shift=0; first_flag=0; second_flag=0;
    init=0,div=1000;

    switch(cmd){
        // initialize
        case SET_OPTION:
            if (copy_from_user(&init, (int*)arg, sizeof(int)))
		        return -EFAULT;
            while(1)
            {
                if(init/div)
                {
                    value=init/div;
                    break;
                }
                div/=10; position++;
            }
            original_value=value;
            first_shift=0; second_shift=0; first_flag=0; second_flag=0;
            fnd_write(value,position);
            led_write(value);
            dot_write(value);
            lcd_write(0,0);
            break;
        // command
        case COMMAND:
            if (copy_from_user(&data, (int*)arg, sizeof(int)*2))
		        return -EFAULT;
            interval=data[0];
            mydata.count=data[1]-1;
            del_timer_sync(&mydata.timer);

            mydata.timer.expires = jiffies + (interval * HZ/10);    /* interval 1 equals 0.1 second */
	        mydata.timer.data = (unsigned long)&mydata;
	        mydata.timer.function	= kernel_timer_blink;
            add_timer(&mydata.timer);
    }
    return 0;
}

// device write functions
void fnd_write(int val,int pos)
{
    unsigned char value[4];
	unsigned short int value_short = 0;
    
    value[0]=0; value[1]=0; value[2]=0; value[3]=0;
    value[pos]=val;
    // outw: 16bit unit
    value_short = value[0] << 12 | value[1] << 8 |value[2] << 4 |value[3];
    outw(value_short,(unsigned int)iom_fpga_fnd_addr);	
}
void led_write(int val)
{
	unsigned short _s_value;
    int temp[1+8]={0,128,64,32,16,8,4,2,1};

    _s_value = (unsigned short)temp[val];   
    outw(_s_value, (unsigned int)iom_fpga_led_addr);
}
void dot_write(int val)
{
    int i;
	unsigned char value[10];
	unsigned short int _s_value;
    
    for(i=0;i<10;i++) value[i]=fpga_number[val][i];
	for(i=0;i<10;i++)
    {
        _s_value = value[i] & 0x7F;
		outw(_s_value,(unsigned int)iom_fpga_dot_addr+i*2);
    }
}
void lcd_write(int first,int second)
{
    int i;
    unsigned short int _s_value = 0;
	unsigned char value[33];    /* text_lcd length: 16+16=32 */

	value[32]=0;    
    for(i=0;i<32;i++) value[i]=' ';
    // shift input
    for(i=0;i<8;i++)
    {
        value[first+i]=student_num[i];
    }
    for(i=0;i<12;i++)
    {
        value[16+second+i]=student_name[i];
    }
    // write
	for(i=0;i<32;i++)
    {
        _s_value = (((value[i] & 0xFF) << 8) | (value[i + 1] & 0xFF));
		outw(_s_value,(unsigned int)iom_fpga_text_lcd_addr+i);
        i++;
    }
}
// module insmod, rmmod
int __init dev_init(void)
{
	int result;
	printk("device_init\n");

	result = register_chrdev(DEV_MAJOR, DEV_NAME, &kernel_timer_fops);
	if(result <0) {
		printk( "error %d\n",result);
		return result;
	}

    iom_fpga_fnd_addr = (unsigned char*)ioremap(IOM_FND_ADDRESS, 0x4);
    iom_fpga_led_addr = (unsigned char*)ioremap(IOM_LED_ADDRESS, 0x1);
    iom_fpga_dot_addr = (unsigned char*)ioremap(IOM_FPGA_DOT_ADDRESS, 0x10);
    iom_fpga_text_lcd_addr = (unsigned char*)ioremap(IOM_FPGA_TEXT_LCD_ADDRESS, 0x32);

    printk( "dev_file : /dev/%s , major : %d\n",DEV_NAME,DEV_MAJOR);
	init_timer(&(mydata.timer));

	printk("init module\n");
	return 0;
}

void __exit dev_exit(void)
{
	printk("kernel_timer_exit\n");
	kernel_timer_usage = 0;
	del_timer_sync(&mydata.timer);

    iounmap(iom_fpga_fnd_addr);
    iounmap(iom_fpga_led_addr);
    iounmap(iom_fpga_dot_addr);
    iounmap(iom_fpga_text_lcd_addr);

	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("author");
