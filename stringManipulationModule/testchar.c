#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "testChar"
#define CLASS_NAME "test"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shaneka Lewis");
MODULE_DESCRIPTION("Test Device Driver");
MODULE_VERSION("0.1");

static int majorNumber;
static struct class *testcharClass = NULL;
static struct device *testcharDevice = NULL;

static DEFINE_MUTEX(testchar_mutex);
static char message[256] = {0};
static char rmessage[256] = {0}; //to support multiple reads with diff return types     
static short size_of_message;
static int return_type;
static int pos;        

static int dev_open(struct inode *inode, struct file *file);
static int dev_release(struct inode *inode, struct file *file);
static ssize_t dev_write(struct file *file, const char *buf, size_t count, loff_t * ppos);
static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ptr);
static long dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg);

struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
	.unlocked_ioctl = dev_ioctl,
};

static int __init testchar_init(void) {
	printk(KERN_INFO "TestChar: Initializing the TestDriver LKM\n");
	
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "TestChar failed to register a major number\n");
		return majorNumber;
	}
	
	printk(KERN_INFO "TestChar: registered correctly with major number %d\n", majorNumber);
	
	testcharClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(testcharClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "TestChar failed to register device class\n");
		return PTR_ERR(testcharClass);
	}
	
	printk(KERN_INFO "TestChar: device class registered correctly\n");
	
	testcharDevice = device_create(testcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(testcharDevice)) {
		class_destroy(testcharClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "TestChar failed to create the device\n");
		return PTR_ERR(testcharDevice);
	}
	
	printk(KERN_INFO "TestChar: device class registered correctly\n");

	mutex_init(&testchar_mutex);
	printk(KERN_INFO "TestChar: mutex was initalized\n");

	return 0;
}

static void __exit testchar_exit(void) {
	mutex_destroy(&testchar_mutex);
	device_destroy(testcharClass, MKDEV(majorNumber, 0));
	class_unregister(testcharClass);
	class_destroy(testcharClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "TestChar: Goodbye from the LKM\n");
}

module_init(testchar_init);
module_exit(testchar_exit);

static int dev_open(struct inode *inode, struct file *file) {
 	
	if (!mutex_trylock(&testchar_mutex)) {
		return -EBUSY;
	}

	memset(message, '\0', 256);
	memset(rmessage, '\0', 256);
	size_of_message = 0;

	printk(KERN_INFO "TestChar: successful\n");
 	return 0;
 }		

static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ptr) {
	//to clear user buffer
	pos = sizeof(buf);
	memset(buf, '\0', pos);
	memset(rmessage, '\0', 256);

	//should handle if something hasn't been written yet
	while (count > size_of_message) {
		count--;
	}
	
	switch(return_type) {
		case 1:
			//all lower
			for (pos = 0; pos < size_of_message; ++pos) {
				rmessage[pos] = tolower(message[pos]);
			}
			break;
		case 2:
			//all upper
			for (pos = 0; pos < size_of_message; ++pos) {
				rmessage[pos] = toupper(message[pos]);
			}
			break;
		case 3:
			//capital
			rmessage[0] = toupper(message[0]);
			for (pos = 1; pos < size_of_message; ++pos) {
				if (message[pos - 1] == ' ') {
					rmessage[pos] = toupper(message[pos]);
				} else {
					rmessage[pos] = tolower(message[pos]);
				}
			}
			break;
		default:
			//regular formal
			for (pos = 0; pos < size_of_message; ++pos) {
				rmessage[pos] = message[pos];
			}
			break;
	}

	//copy_to_user(userspace buf, kernelspace buf, num bytes)
	if (copy_to_user(buf, rmessage, count)) {
		return -EFAULT;
	}
	return_type = 0; //reset after read
	
	printk(KERN_INFO "dev_read: returning %d bytes\n", (int)count);
 	return count;
}
static ssize_t dev_write(struct file *file, const char *buf, size_t count, loff_t * ppos) {
	//clear message
	memset(message, '\0', 256);
	size_of_message = 0;
	
	if (count > 256) {
		while (count > 256) {
			--count;
		}
	}

	//copy_from_user(kernelspace buf, userspace buf, num bytes)
	if (copy_from_user(message, buf, count)) {
		printk(KERN_INFO "dev_write: failed\n");
		return -1;
	}

	size_of_message = count;
	return_type = 0;
	  
	printk(KERN_INFO "dev_write: accepting %d bytes\n", size_of_message);
 	return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
	//clear message
	memset(message, '\0', 256);
	memset(rmessage, '\0', 256);
	size_of_message = 0;

 	printk(KERN_INFO "dev_release: successful\n");

	mutex_unlock(&testchar_mutex);
	
 	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
 	printk("TestChar_ioctl: cmd=%d, arg=%ld\n", cmd, arg);

	//ignoring cmd?

	switch (arg) {
		case 1:
			return_type = 1;
			break;
		case 2:
			return_type = 2;
			break;
		case 3:
			return_type = 3;
			break;
		default:
			//not a valid command
			return -1;
	}
	
	return 0;
}
