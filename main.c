#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include "ioc_hw5.h"

MODULE_LICENSE("GPL");

#define PREFIX_TITLE "OS_AS5"


// DMA
#define DMA_BUFSIZE 64
#define DMASTUIDADDR 0x0        // Student ID
#define DMARWOKADDR 0x4         // RW function complete
#define DMAIOCOKADDR 0x8        // ioctl function complete
#define DMAIRQOKADDR 0xc        // ISR function complete
#define DMACOUNTADDR 0x10       // interrupt count function complete
#define DMAANSADDR 0x14         // Computation answer
#define DMAREADABLEADDR 0x18    // READABLE variable for synchronize
#define DMABLOCKADDR 0x1c       // Blocking or non-blocking IO
#define DMAOPCODEADDR 0x20      // data.a opcode
#define DMAOPERANDBADDR 0x21    // data.b operand1
#define DMAOPERANDCADDR 0x25    // data.c operand2
void *dma_buf;


static int dev_major;
static int dev_minor;
static struct cdev *dev_cdevp;
// Declaration for file operations
static ssize_t drv_read(struct file *filp, char __user *buffer, size_t, loff_t*);
static int drv_open(struct inode*, struct file*);
static ssize_t drv_write(struct file *filp, const char __user *buffer, size_t, loff_t*);
static int drv_release(struct inode*, struct file*);
static long drv_ioctl(struct file *, unsigned int , unsigned long );

// cdev file_operations
static struct file_operations fops = {
      owner: THIS_MODULE,
      read: drv_read,
      write: drv_write,
      unlocked_ioctl: drv_ioctl,
      open: drv_open,
      release: drv_release,
};

// in and out function
void myoutc(unsigned char data,unsigned short int port);
void myouts(unsigned short data,unsigned short int port);
void myouti(unsigned int data,unsigned short int port);
unsigned char myinc(unsigned short int port);
unsigned short myins(unsigned short int port);
unsigned int myini(unsigned short int port);

// Work routine
static struct work_struct *work;

// For input data structure
struct DataIn {
    char a;
    int b;
    short c;
} *dataIn;


// Arithmetic funciton
static void drv_arithmetic_routine(struct work_struct* ws);


// Input and output data from/to DMA
void myoutc(unsigned char data,unsigned short int port) {
    *(volatile unsigned char*)(dma_buf+port) = data;
}
void myouts(unsigned short data,unsigned short int port) {
    *(volatile unsigned short*)(dma_buf+port) = data;
}
void myouti(unsigned int data,unsigned short int port) {
    *(volatile unsigned int*)(dma_buf+port) = data;
}
unsigned char myinc(unsigned short int port) {
    return *(volatile unsigned char*)(dma_buf+port);
}
unsigned short myins(unsigned short int port) {
    return *(volatile unsigned short*)(dma_buf+port);
}
unsigned int myini(unsigned short int port) {
    return *(volatile unsigned int*)(dma_buf+port);
}


static int drv_open(struct inode* ii, struct file* ff) {
	try_module_get(THIS_MODULE);
    	printk("%s:%s(): device open\n", PREFIX_TITLE, __func__);
	return 0;
}
static int drv_release(struct inode* ii, struct file* ff) {
	module_put(THIS_MODULE);
    	printk("%s:%s(): device close\n", PREFIX_TITLE, __func__);
	return 0;
}
static ssize_t drv_read(struct file *filp, char __user *buffer, size_t ss, loff_t* lo) {
	/* Implement read operation for your device */
	int answer = myini(DMAANSADDR);
	myouti(0,DMAREADABLEADDR);
	printk("%s:%s(): ans = %d\n", PREFIX_TITLE, __func__,answer);
	copy_to_user(buffer,(int *)&answer,ss);
	return 0;
}
static ssize_t drv_write(struct file *filp, const char __user *buffer, size_t ss, loff_t* lo) {
	/* Implement write operation for your device */
	myouti(0,DMAREADABLEADDR);
	struct DataIn data;
	copy_from_user((struct DataIn *)&data,buffer, ss);
	myoutc(data.a,DMAOPCODEADDR);
	myouti(data.b,DMAOPERANDBADDR);
	myouts(data.c,DMAOPERANDCADDR);

	int IOMode;
	IOMode = myini(DMABLOCKADDR);
	printk("%s:%s(): queue work\n", PREFIX_TITLE, __func__);
	INIT_WORK(work, drv_arithmetic_routine);

	if(IOMode) {
		// Blocking IO
		
		printk("%s:%s(): block\n", PREFIX_TITLE, __func__);
		schedule_work(work);
		flush_scheduled_work();
		
    	} 
	else {
		// Non-locking IO
		
		//printk("%s,%s(): non-blocking\n",PREFIX_TITLE, __func__);

		schedule_work(work);
		
		
   	 }
	
	return 0;
}
static long drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	/* Implement ioctl setting for your device */
	int ret;
	int readflag = 0;
	get_user(ret,(int *)arg);
	switch (cmd)
	{
		case _IOW(HW5_IOC_MAGIC, 1, int):
			myouti(ret, DMASTUIDADDR);
			if(ret >= 0){
				printk("%s:%s(): My STUID is = %ld\n", PREFIX_TITLE, __func__,ret);
			}
			break;
		case _IOW(HW5_IOC_MAGIC, 2, int):
			myouti(ret, DMARWOKADDR);
			if(ret==1){
				printk("%s:%s(): RW OK\n", PREFIX_TITLE, __func__);
			}
			break;
		case _IOW(HW5_IOC_MAGIC, 3, int):
			myouti(ret, DMAIOCOKADDR);
			if(ret==1){
				printk("%s:%s(): IOC OK\n", PREFIX_TITLE, __func__);
			}
			break;
		case _IOW(HW5_IOC_MAGIC, 5, int):
			myouti(ret, DMABLOCKADDR);
			if(ret == 1){
				printk("%s:%s(): Blocking IO\n", PREFIX_TITLE, __func__);
			}else if(ret == 0){
				printk("%s:%s(): Non-Blocking IO\n", PREFIX_TITLE, __func__);
			}
			break;
		case _IOW(HW5_IOC_MAGIC, 4, int):
			myouti(ret, DMAIOCOKADDR);
			if(ret==1){
				printk("%s:%s(): IRC OK\n", PREFIX_TITLE, __func__);
			}
			break;
		case _IOR(HW5_IOC_MAGIC, 6, int):
			readflag = myini(DMAREADABLEADDR);
			printk("%s:%s(): wait readable 1\n", PREFIX_TITLE, __func__);
			while(true){
				if(readflag == 1){
					break;
				}
				msleep(50);
				readflag = myini(DMAREADABLEADDR);
			}
			put_user(1,(int*)arg);
			break;
		default:
			break;
	}
	return 0;
}

static int primek(int base, short nth){
	int fnd=0;
    int i, num, isPrime;

    num = base;
    while(fnd != nth) {
        isPrime=1;
        num++;
        for(i=2;i<=num/2;i++) {
            if(num%i == 0) {
                isPrime=0;
                break;
            }
        }
        
        if(isPrime) {
            fnd++;
        }
    }
    return num;
}

static void drv_arithmetic_routine(struct work_struct* ws) {
	/* Implement arthemetic routine */
	char operator = myinc(0x20);
	int operand1 = myini(0x21);
	short operand2 = myins(0x25);
	int ans;
	switch (operator)
	{
		case '+':
            ans=operand1+operand2;
            break;
        case '-':
            ans=operand1-operand2;
            break;
        case '*':
            ans=operand1*operand2;
            break;
        case '/':
            ans=operand1/operand2;
            break;
        case 'p':
            ans = primek(operand1, operand2);
            break;
        default:
            ans=0;
	}
	myouti(ans,0x14);
	myouti(1,DMAREADABLEADDR);
	printk("%s,%s(): %d %c %hd = %d\n",PREFIX_TITLE, __func__,operand1,operator,operand2,ans);
}

static int __init init_modules(void) {
    
	printk("%s:%s():...............Start...............\n", PREFIX_TITLE, __func__);

	/* Register chrdev */ 
	dev_t dev;
	int ret = 0;
	ret = alloc_chrdev_region(&dev, 0, 1, "mydev");
	
	if(ret)
	{
		printk("Cannot alloc chrdev\n");
		return ret;
	}
	
	dev_major = MAJOR(dev);
	dev_minor = MINOR(dev);
	
	printk("%s:%s():register chrdev(%d,%d)\n",PREFIX_TITLE,__func__,dev_major,dev_minor);
	

	/* Init cdev and make it alive */
	dev_cdevp = cdev_alloc();
	cdev_init(dev_cdevp, &fops);
	dev_cdevp->owner = THIS_MODULE;
	ret = cdev_add(dev_cdevp, MKDEV(dev_major, dev_minor), 1);
	if(ret < 0)
	{
		printk("Add chrdev failed\n");
		return ret;
	}

	/* Allocate DMA buffer */
	dma_buf = kzalloc(DMA_BUFSIZE, GFP_KERNEL);
	printk("%s:%s():allocate dma buffer\n",PREFIX_TITLE,__func__);
	/* Allocate work routine */
	work = kmalloc(sizeof(typeof(*work)), GFP_KERNEL);
	return 0;
}

static void __exit exit_modules(void) {

	dev_t dev;
	/* Free DMA buffer when exit modules */
	kfree(dma_buf);
	printk("%s:%s():free dma buffer\n",PREFIX_TITLE,__func__);
	/* Delete character device */
	dev = MKDEV(dev_major, dev_minor);
	cdev_del(dev_cdevp);
	unregister_chrdev_region(dev, 1);
	printk("%s:%s():unregister chrdev\n",PREFIX_TITLE,__func__);
	/* Free work routine */
	kfree(work);


	printk("%s:%s():..............End..............\n", PREFIX_TITLE, __func__);
}

module_init(init_modules);
module_exit(exit_modules);
