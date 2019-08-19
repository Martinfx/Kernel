#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

/**
 * Device number
 */
static dev_t device_num;

/**
 * Device structure
 */
static struct cdev c_dev;

/**
 * Device class
 */
static struct class *cl;

static int open_device(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Module: open()\n");
    return 0;
}

static int close_device(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Module: close()\n");
    return 0;
}

static ssize_t read_device(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "Module: read()\n");
    return 0;
}

static ssize_t write_device(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "Module: write()\n");
    return len;
}

static struct file_operations pugs_fops =
{
    .owner   = THIS_MODULE,
    .open    = open_device,
    .read    = read_device,
    .write   = write_device,
    .release = close_device
};

/**
 * Initialize kernel module
 */
int __init drive_init(void)
{
  printk(KERN_INFO "Module registered");

  if(alloc_chrdev_region(&device_num, 0, 1, "module") < 0) {
	return -1;
  }

  if((cl = class_create(THIS_MODULE, "null")) == NULL) {
    unregister_chrdev_region(device_num, 1);
    return -1;
  }

  if(device_create(cl, NULL, device_num, NULL, "mynull") == NULL) {
    class_destroy(cl);
    unregister_chrdev_region(device_num, 1);
    return -1;
  }

  cdev_init(&c_dev, &pugs_fops);

  if(cdev_add(&c_dev, device_num, 1) == -1) {
    device_destroy(cl, device_num);
    class_destroy(cl);
    unregister_chrdev_region(device_num, 1);
    return -1;
  }

  return 0;
}

/**
 * Cleanup kernel module
 */
void __exit drive_exit(void)
{
    cdev_del(&c_dev);
    device_destroy(cl, device_num);
    class_destroy(cl);
    unregister_chrdev_region(device_num, 1);
    printk(KERN_INFO "Good bay : module unregistered");
}

module_init(drive_init);
module_exit(drive_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Null drive");
