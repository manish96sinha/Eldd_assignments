#include <linux/module.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <asm-generic/io.h>


/*
In newer kernel version, following code cause OOPs message and kernel panic due to security concerns.
inb(), outb() are not permitted unless request_region() acquired ioports.
*/

#define KBD_DATA_REG     0x60
#define KBD_CTRL_REG     0x64 

struct timer_list mytimer;
int ticks;
int count = 0;

void mytimer_function(struct timer_list *ptimer) {
	printk(KERN_INFO " %s : mytimer_function : count = %d\n", THIS_MODULE->name, count);
	count++;

	// enable keyboard
	while(inb(KBD_CTRL_REG) & 0x02)
		mdelay(1);
	outb(0xAE, KBD_CTRL_REG);
}

static __init int desd_init(void) {
	int sec = 20;
	ticks = sec * HZ;		

	timer_setup(&mytimer, mytimer_function, 0);
	mytimer.expires = jiffies + ticks;
	add_timer(&mytimer);

	// disable keyboard
	while(inb(KBD_CTRL_REG) & 0x02)
		mdelay(1);
	outb(0xAD, KBD_CTRL_REG);

	printk(KERN_INFO " %s : Timer initialisation is done successfully\n", THIS_MODULE->name);
	return 0;
}

static __exit void desd_exit(void) {
	del_timer_sync(&mytimer);
	printk(KERN_INFO " %s : Timer deinitialisation is done successfully\n", THIS_MODULE->name);
}

module_init(desd_init);
module_exit(desd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DESD @ Sunbeam");
MODULE_DESCRIPTION("Disable Keyboard for some time.");

