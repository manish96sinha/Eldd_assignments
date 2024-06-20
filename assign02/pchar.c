#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "pchar_ioctl.h"

static int pchar_open(struct inode *, struct file *);
static int pchar_close(struct inode *, struct file *);
static ssize_t pchar_read(struct file *, char *, size_t, loff_t *);
static ssize_t pchar_write(struct file *, const char *, size_t, loff_t *);
static long pchar_ioctl(struct file *, unsigned int, unsigned long);

#define MAX 32
static struct kfifo buf;
static int major;
static dev_t devno;
static struct class *pclass;
static struct cdev cdev;
static struct file_operations pchar_fops = {
    .owner = THIS_MODULE,
    .open = pchar_open,
    .release = pchar_close,
    .read = pchar_read,
    .write = pchar_write,
    .unlocked_ioctl = pchar_ioctl
};

static __init int pchar_init(void) {
    int ret, minor;
    struct device *pdevice;

    printk(KERN_INFO "%s: pchar_init() called.\n", THIS_MODULE->name);
    ret = kfifo_alloc(&buf, MAX, GFP_KERNEL);
    if (ret != 0) {
        printk(KERN_ERR "%s: kfifo_alloc() failed.\n", THIS_MODULE->name);
        goto kfifo_alloc_failed;
    }
    printk(KERN_INFO "%s: kfifo_alloc() successfully created device.\n", THIS_MODULE->name);
    
    ret = alloc_chrdev_region(&devno, 0, 1, "pchar");
    if (ret != 0) {
        printk(KERN_ERR "%s: alloc_chrdev_region() failed.\n", THIS_MODULE->name);
        goto alloc_chrdev_region_failed;
    }
    major = MAJOR(devno);
    minor = MINOR(devno);
    printk(KERN_INFO "%s: alloc_chrdev_region() allocated device number %d/%d.\n", THIS_MODULE->name, major, minor);

    pclass = class_create(THIS_MODULE, "pchar_class");
    if (IS_ERR(pclass)) {
        printk(KERN_ERR "%s: class_create() failed.\n", THIS_MODULE->name);
          ret = -1;
        goto class_create_failed;
    }
    printk(KERN_INFO "%s: class_create() created device class.\n", THIS_MODULE->name);
    
    pdevice = device_create(pclass, NULL, devno, NULL, "pchar%d", 0);
    if (IS_ERR(pdevice)) {
        printk(KERN_ERR "%s: device_create() failed.\n", THIS_MODULE->name);
        
        ret = -1;
        goto device_create_failed;
    }
    printk(KERN_INFO "%s: device_create() created device file.\n", THIS_MODULE->name);

    cdev_init(&cdev, &pchar_fops);
    ret = cdev_add(&cdev, devno, 1);
    if (ret != 0) {
        printk(KERN_ERR "%s: cdev_add() failed to add cdev in kernel db.\n", THIS_MODULE->name);
        goto cdev_add_failed;
    }
    printk(KERN_INFO "%s: cdev_add() added device in kernel db.\n", THIS_MODULE->name);

    return 0;

cdev_add_failed:
    device_destroy(pclass, devno);
device_create_failed:
    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, 1);
alloc_chrdev_region_failed:
    kfifo_free(&buf);
kfifo_alloc_failed:
    return ret;
}

static __exit void pchar_exit(void) {
    printk(KERN_INFO "%s: pchar_exit() called.\n", THIS_MODULE->name);
    cdev_del(&cdev);
    printk(KERN_INFO "%s: cdev_del() removed device from kernel db.\n", THIS_MODULE->name);
    device_destroy(pclass, devno);
    printk(KERN_INFO "%s: device_destroy() destroyed device file.\n", THIS_MODULE->name);
    class_destroy(pclass);
    printk(KERN_INFO "%s: class_destroy() destroyed device class.\n", THIS_MODULE->name);
    unregister_chrdev_region(devno, 1);
    printk(KERN_INFO "%s: unregister_chrdev_region() released device number.\n", THIS_MODULE->name);
    kfifo_free(&buf);
    printk(KERN_INFO "%s: kfifo_free() destroyed device.\n", THIS_MODULE->name);
}

static int pchar_open(struct inode *pinode, struct file *pfile) {
    printk(KERN_INFO "%s: pchar_open() called.\n", THIS_MODULE->name);
    return 0;
}

static int pchar_close(struct inode *pinode, struct file *pfile) {
    printk(KERN_INFO "%s: pchar_close() called.\n", THIS_MODULE->name);
    return 0;
}

static ssize_t pchar_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset) {
    int nbytes, ret;
    printk(KERN_INFO "%s: pchar_read() called.\n", THIS_MODULE->name);
    ret = kfifo_to_user(&buf, ubuf, size, &nbytes);
    if (ret < 0) {
        printk(KERN_ERR "%s: pchar_read() failed to copy data from kernel space using kfifo_to_user().\n", THIS_MODULE->name);
        return ret;
    }
    printk(KERN_INFO "%s: pchar_read() copied %d bytes to user space.\n", THIS_MODULE->name, nbytes);
    return nbytes;
}

static ssize_t pchar_write(struct file *pfile, const char *ubuf, size_t size, loff_t *poffset) {
    int nbytes, ret;
    printk(KERN_INFO "%s: pchar_write() called.\n", THIS_MODULE->name);
    ret = kfifo_from_user(&buf, ubuf, size, &nbytes);
    if (ret < 0) {
        printk(KERN_ERR "%s: pchar_write() failed to copy data in kernel space using kfifo_from_user().\n", THIS_MODULE->name);
        return ret;
    }
    printk(KERN_INFO "%s: pchar_write() copied %d bytes from user space.\n", THIS_MODULE->name, nbytes);
    return nbytes;
}

static long pchar_ioctl(struct file *pfile, unsigned int cmd, unsigned long param) {
    info_t info;
    int ret = 0;
    char *temp_buf;

    switch (cmd) {
    case FIFO_CLEAR:
        printk(KERN_INFO "%s: pchar_ioctl() fifo clear.\n", THIS_MODULE->name);
        kfifo_reset(&buf);
        break;
    case FIFO_INFO:
        printk(KERN_INFO "%s: pchar_ioctl() get fifo info.\n", THIS_MODULE->name);
        info.size = kfifo_size(&buf);
        info.avail = kfifo_avail(&buf);
        info.len = kfifo_len(&buf);
        if (copy_to_user((void*)param, &info, sizeof(info_t))) {
            ret = -EFAULT;
        }
        break;
    case FIFO_RESIZE:
        printk(KERN_INFO "%s: pchar_ioctl() resize fifo.\n", THIS_MODULE->name);
         // allocate temp buffer of fifo length - kmalloc(), kifo_len()
        temp_buf = kmalloc(kfifo_len(&buf), GFP_KERNEL);
        if (!temp_buf) {
            printk(KERN_ERR "%s: kmalloc() failed.\n", THIS_MODULE->name);
            ret = -ENOMEM;
            break;
        }
          // copy fifo data into that buffer - kfifo_out()
        kfifo_out(&buf, temp_buf, kfifo_len(&buf));
         // release kfifo memory - kfifo_free()
        kfifo_free(&buf);
         // allocate kfifo with new size - kfifo_alloc() with param
        ret = kfifo_alloc(&buf, param, GFP_KERNEL);
        if (ret != 0) {
            printk(KERN_ERR "%s: kfifo_alloc() failed.\n", THIS_MODULE->name);
            kfree(temp_buf);
            break;
        }
          // copy temp buffer data back to fifo - kfifo_in()
        kfifo_in(&buf, temp_buf, kfifo_len(&buf));
          // release temp buffer - kfree()
        kfree(temp_buf);
        printk(KERN_INFO "%s: pchar_ioctl() - fifo resized (%d).\n", THIS_MODULE->name, kfifo_size(&buf));
        break;
    default:
        printk(KERN_ERR "%s: pchar_ioctl() unsupported command.\n", THIS_MODULE->name);
        ret = -EINVAL;
    }
    return ret;
}

module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish ");
MODULE_DESCRIPTION("pseudo pchar driver with kfifo as device.");

