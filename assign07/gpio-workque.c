#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define LED_GPIO    49
#define SWITCH_GPIO    115

static int bbb_gpio_open(struct inode *, struct file *);
static int bbb_gpio_close(struct inode *, struct file *);
static ssize_t bbb_gpio_read(struct file *, char *, size_t, loff_t *);
static ssize_t bbb_gpio_write(struct file *, const char *, size_t, loff_t *);

#define MAX 32
static int led_state;
static int major;
static dev_t devno;
static struct class *pclass;
static struct cdev cdev;
static int irq;

//static struct tasklet_struct tasklet;
static struct work_struct workqueue;

static struct file_operations bbb_gpio_fops = {
    .owner = THIS_MODULE,
    .open = bbb_gpio_open,
    .release = bbb_gpio_close,
    .read = bbb_gpio_read,
    .write = bbb_gpio_write
};
//DECLARE_WORK(workqueue,work_handler);
/*
static void tasklet_handler(unsigned long param) {
	led_state = !led_state;
	gpio_set_value(LED_GPIO, led_state);	
    printk(KERN_INFO "%s: tasklet_handler() called.\n", THIS_MODULE->name);
}
*/
 void work_handler(struct work_struct *data)
{
    for(int i=0;i<10;i++)
    {
    led_state = !led_state;
	gpio_set_value(LED_GPIO, led_state);
    mdelay(1000);
    }
    printk(KERN_INFO "%s: work_handler() called.\n", THIS_MODULE->name);
}

static irqreturn_t switch_isr(int irq, void *param) {
    printk(KERN_INFO "%s: switch_isr() called.\n", THIS_MODULE->name);
	schedule_work(&workqueue);
	return IRQ_HANDLED;
}

static __init int bbb_gpio_init(void) {
    int ret, minor;
    struct device *pdevice;
    
    printk(KERN_INFO "%s: bbb_gpio_init() called.\n", THIS_MODULE->name);

    ret = alloc_chrdev_region(&devno, 0, 1, "bbb_gpio");
    if(ret != 0) {
        printk(KERN_ERR "%s: alloc_chrdev_region() failed.\n", THIS_MODULE->name);
        goto alloc_chrdev_region_failed;
    }
    major = MAJOR(devno);
    minor = MINOR(devno);
    printk(KERN_INFO "%s: alloc_chrdev_region() allocated device number %d/%d.\n", THIS_MODULE->name, major, minor);

    pclass = class_create(THIS_MODULE, "bbb_gpio_class");
    if(IS_ERR(pclass)) {
        printk(KERN_ERR "%s: class_create() failed.\n", THIS_MODULE->name);
        ret = -1;
        goto class_create_failed;
    }
    printk(KERN_INFO "%s: class_create() created device class.\n", THIS_MODULE->name);

    pdevice = device_create(pclass, NULL, devno, NULL, "bbb_gpio%d", 0);
    if(IS_ERR(pdevice)) {
        printk(KERN_ERR "%s: device_create() failed.\n", THIS_MODULE->name);
        ret = -1;
        goto device_create_failed;
    }
    printk(KERN_INFO "%s: device_create() created device file.\n", THIS_MODULE->name);

    cdev_init(&cdev, &bbb_gpio_fops);
    ret = cdev_add(&cdev, devno, 1);  
    if(ret != 0) {
        printk(KERN_ERR "%s: cdev_add() failed to add cdev in kernel db.\n", THIS_MODULE->name);
        goto cdev_add_failed;
    }
    printk(KERN_INFO "%s: cdev_add() added device in kernel db.\n", THIS_MODULE->name);

    bool valid = gpio_is_valid(LED_GPIO);
    if(!valid) {
        printk(KERN_ERR "%s: GPIO pin %d doesn't exist.\n", THIS_MODULE->name, LED_GPIO);
        ret = -1;
        goto gpio_invalid;
    }
    printk(KERN_INFO "%s: GPIO pin %d exists.\n", THIS_MODULE->name, LED_GPIO);

    ret = gpio_request(LED_GPIO, "bbb-led");
    if(ret != 0) {
        printk(KERN_ERR "%s: GPIO pin %d is busy.\n", THIS_MODULE->name, LED_GPIO);
        goto gpio_invalid;
    }
    printk(KERN_INFO "%s: GPIO pin %d acquired.\n", THIS_MODULE->name, LED_GPIO);

    led_state = 1;
    ret = gpio_direction_output(LED_GPIO, led_state);
    if(ret != 0) {
        printk(KERN_ERR "%s: GPIO pin %d direction not set.\n", THIS_MODULE->name, LED_GPIO);
        goto gpio_direction_failed;
    }
    printk(KERN_INFO "%s: GPIO pin %d direction set to OUTPUT.\n", THIS_MODULE->name, LED_GPIO);

    valid = gpio_is_valid(SWITCH_GPIO);
    if(!valid) {
        printk(KERN_ERR "%s: GPIO pin %d doesn't exist.\n", THIS_MODULE->name, SWITCH_GPIO);
        ret = -1;
        goto switch_gpio_invalid;
    }
    printk(KERN_INFO "%s: GPIO pin %d exists.\n", THIS_MODULE->name, SWITCH_GPIO);

    ret = gpio_request(SWITCH_GPIO, "bbb-switch");
    if(ret != 0) {
        printk(KERN_ERR "%s: GPIO pin %d is busy.\n", THIS_MODULE->name, SWITCH_GPIO);
        goto switch_gpio_invalid;
    }
    printk(KERN_INFO "%s: GPIO pin %d acquired.\n", THIS_MODULE->name, SWITCH_GPIO);

    ret = gpio_direction_input(SWITCH_GPIO);
    if(ret != 0) {
        printk(KERN_ERR "%s: GPIO pin %d direction not set.\n", THIS_MODULE->name, SWITCH_GPIO);
        goto switch_gpio_direction_failed;
    }
    printk(KERN_INFO "%s: GPIO pin %d direction set to INPUT.\n", THIS_MODULE->name, SWITCH_GPIO);
    
	// set debounce -- optional

	// get the gpio interrupt number
	irq = gpio_to_irq(SWITCH_GPIO);
	ret = request_irq(irq, switch_isr, IRQF_TRIGGER_RISING, "bbb_switch", NULL);
	if(ret != 0) {
        printk(KERN_ERR "%s: GPIO pin %d ISR registration failed.\n", THIS_MODULE->name, SWITCH_GPIO);
        goto switch_gpio_direction_failed;		
	}
    printk(KERN_INFO "%s: GPIO pin %d registerd ISR on irq line %d.\n", THIS_MODULE->name, SWITCH_GPIO, irq);

	// initialize tasklet
	/*tasklet_init(&tasklet, tasklet_handler, 0);
    printk(KERN_INFO "%s: tasklet is initialized.\n", THIS_MODULE->name);
    */
   //INIT_WORK(struct work_struct *work, void (*func)(void *), void *data);
   INIT_WORK(&workqueue, work_handler);
    printk(KERN_INFO "%s: Work_queue is initialized.\n", THIS_MODULE->name);

	return 0;

switch_gpio_direction_failed:
    gpio_free(SWITCH_GPIO);
switch_gpio_invalid:
gpio_direction_failed:
    gpio_free(LED_GPIO);
gpio_invalid:
    cdev_del(&cdev);
cdev_add_failed:
    device_destroy(pclass, devno);
device_create_failed:
    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, 1);
alloc_chrdev_region_failed:
    return ret;
}

static __exit void bbb_gpio_exit(void) {
    printk(KERN_INFO "%s: bbb_gpio_exit() called.\n", THIS_MODULE->name);
    free_irq(irq, NULL);
    printk(KERN_INFO "%s: GPIO pin %d ISR released.\n", THIS_MODULE->name, SWITCH_GPIO);
	gpio_free(SWITCH_GPIO);
    printk(KERN_INFO "%s: GPIO pin %d released.\n", THIS_MODULE->name, SWITCH_GPIO);
    gpio_free(LED_GPIO);
    printk(KERN_INFO "%s: GPIO pin %d released.\n", THIS_MODULE->name, LED_GPIO);
    cdev_del(&cdev);
    printk(KERN_INFO "%s: cdev_del() removed device from kernel db.\n", THIS_MODULE->name);
    device_destroy(pclass, devno);
    printk(KERN_INFO "%s: device_destroy() destroyed device file.\n", THIS_MODULE->name);
    class_destroy(pclass);
    printk(KERN_INFO "%s: class_destroy() destroyed device class.\n", THIS_MODULE->name);
    unregister_chrdev_region(devno, 1);
    printk(KERN_INFO "%s: unregister_chrdev_region() released device number.\n", THIS_MODULE->name);
}


static int bbb_gpio_open(struct inode *pinode, struct file *pfile) {
    printk(KERN_INFO "%s: bbb_gpio_open() called.\n", THIS_MODULE->name);
    return 0;
}

static int bbb_gpio_close(struct inode *pinode, struct file *pfile) {
    printk(KERN_INFO "%s: bbb_gpio_close() called.\n", THIS_MODULE->name);
    return 0;
}

static ssize_t bbb_gpio_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset) {
    int ret, switch_state;
    char kbuf[4];
	switch_state = gpio_get_value(SWITCH_GPIO);
    printk(KERN_INFO "%s: bbb_gpio_read() called.\n", THIS_MODULE->name);
    sprintf(kbuf, "%d\n", switch_state);
    ret = 2 - copy_to_user(ubuf, kbuf, 2);
    printk(KERN_INFO "%s: GPIO pin %d SWITCH state read = %d.\n", THIS_MODULE->name, SWITCH_GPIO, switch_state);
    return ret;
}

static ssize_t bbb_gpio_write(struct file *pfile, const char *ubuf, size_t size, loff_t *poffset) {
    int ret;
    char kbuf[2] = "";
    printk(KERN_INFO "%s: bbb_gpio_write() called.\n", THIS_MODULE->name);
    ret = 1 - copy_from_user(kbuf, ubuf, 1);
    if(ret > 0) {
        if(kbuf[0] == '1') {
            led_state = 1;
            gpio_set_value(LED_GPIO, 1);
            printk(KERN_INFO "%s: GPIO pin %d LED ON.\n", THIS_MODULE->name, LED_GPIO);
        } else if(kbuf[0] == '0') {
            led_state = 0;
            gpio_set_value(LED_GPIO, 0);
            printk(KERN_INFO "%s: GPIO pin %d LED OFF.\n", THIS_MODULE->name, LED_GPIO);
        }
        else
            printk(KERN_INFO "%s: GPIO pin %d LED NO state change.\n", THIS_MODULE->name, LED_GPIO);
    } 
    return size;
}

module_init(bbb_gpio_init);
module_exit(bbb_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish");
MODULE_DESCRIPTION("Simple bbb_gpio driver with kfifo as device.");
