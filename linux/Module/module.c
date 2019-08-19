#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("BSD");
MODULE_VERSION("0.1");

static char *name = "world";
module_param(name, charp, S_IRUGO);

/**
* Initialize kernel module
*/
static int __init hello_init(void)
{
	printk(KERN_INFO "Hello, module from LKM! \n");
	return 0;
}

/**
* Cleanup kernel module
*/
static void __exit hello_exit(void)
{
	printk(KERN_INFO "Goodbye, module from LKM!\n");
}

module_init(hello_init);
module_exit(hello_exit);
