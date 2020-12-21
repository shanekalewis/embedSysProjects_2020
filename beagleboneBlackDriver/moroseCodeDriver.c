#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/syscalls.h>

#define DEBUG 0

#define DEVICE_NAME "moroseCodeDriver"
#define CLASS_NAME "moroseCode"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shaneka Lewis");
MODULE_DESCRIPTION("Morose Code Driver for the BeagleBone Black");
MODULE_VERSION("0.1");

static int majorNumber;
static struct class *moroseCodeClass = NULL;
static struct device *moroseCodeDevice = NULL;

static DEFINE_MUTEX(moroseCode_mutex);
static int delayLength;
static char message[256] = {0};
static char *translatedLetter = NULL;    
static short messageLen;
static short numSegments;
static short messagePos;
static short translatedLetterPos;
static int isOn;
static int newMessage;

#define CQ_DEFAULT	0

#define LED_START_ADDR  0x4804C000 //need to learn how to read datasheet
#define LED_END_ADDR  0x4804e000
#define LED_SIZE (LED_END_ADDR - LED_START_ADDR)
#define GPIO_CLEARDATAOUT 0x190
#define GPIO_SETDATAOUT 0X194
#define USR0 (1 << 21)

static volatile void *gpioAddr;
static volatile unsigned int *gpioSetDataOutAddr;
static volatile unsigned int *gpioClearDataOutAddr;

static int dev_open(struct inode *inode, struct file *file);
static int dev_release(struct inode *inode, struct file *file);
static ssize_t dev_write(struct file *file, const char *buf, size_t count, loff_t * ppos);
static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ptr);
static long dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg);

static struct timer_list timer;

//To use filesystem from the kernel
#define USER_LED_PATH "/sys/class/leds/beaglebone:green:usr0/trigger"
struct file *fp = NULL;
static int openFile(void);
ssize_t writeToFile(void *in, ssize_t inLen);
static int closeFile(void);
static void stopTrigger(void);
static void startTrigger(void);

void ledOn(void);
void ledOff(void);
void blinkPattern(struct timer_list * data);

struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
	.unlocked_ioctl = dev_ioctl,
};

char *morse_code[40] = {"",
".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",
".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",
".--","-..-","-.--","--..","-----",".----","..---","...--","....-",
".....","-....","--...","---..","----.","--..--","-.-.-.","..--.."};


inline char * mcodestring(int asciicode)
{
   char *mc;   // this is the mapping from the ASCII code into the mcodearray of strings.

   if (asciicode > 122)  // Past 'z'
      mc = morse_code[CQ_DEFAULT];
   else if (asciicode > 96)  // Upper Case
      mc = morse_code[asciicode - 96];
   else if (asciicode > 90)  // uncoded punctuation
      mc = morse_code[CQ_DEFAULT];
   else if (asciicode > 64)  // Lower Case 
      mc = morse_code[asciicode - 64];
   else if (asciicode == 63)  // Question Mark
      mc = morse_code[39];    // 36 + 3 
   else if (asciicode > 57)  // uncoded punctuation
      mc = morse_code[CQ_DEFAULT];
   else if (asciicode > 47)  // Numeral
      mc = morse_code[asciicode - 21];  // 27 + (asciicode - 48) 
   else if (asciicode == 46)  // Period
      mc = morse_code[38];  // 36 + 2 
   else if (asciicode == 44)  // Comma
      mc = morse_code[37];   // 36 + 1
   else
      mc = morse_code[CQ_DEFAULT];
   return mc;
}

static int __init moroseCode_init(void) {
    printk(KERN_INFO "MoroseCode: Initializing the MoroseCode LKM\n");
	
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "MoroseCode failed to register a major number\n");
		return majorNumber;
	}
	
	printk(KERN_INFO "MoroseCode: registered correctly with major number %d\n", majorNumber);
	
	moroseCodeClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(moroseCodeClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "MoroseCode failed to register device class\n");
		return PTR_ERR(moroseCodeClass);
	}
	
	printk(KERN_INFO "MoroseCode: device class registered correctly\n");
	
	moroseCodeDevice = device_create(moroseCodeClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(moroseCodeDevice)) {
		class_destroy(moroseCodeClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "MoroseCode failed to create the device\n");
		return PTR_ERR(moroseCodeDevice);
	}
	
	printk(KERN_INFO "MoroseCode: device class registered correctly\n");

	//Covert phy address to memory address - returns ptr
	gpioAddr = ioremap(LED_START_ADDR, LED_END_ADDR - LED_START_ADDR);
	if (!gpioAddr) {
		printk(KERN_INFO "MoroseCode: Error - unable to remap memory\n");
	} else {
		gpioClearDataOutAddr = gpioAddr + GPIO_CLEARDATAOUT;
		gpioSetDataOutAddr = gpioAddr + GPIO_SETDATAOUT;
	}

	if (DEBUG) {
		printk(KERN_INFO "MoroseCode: debugging start\n");
		stopTrigger();
		msleep(5000);
		ledOn();
		msleep(5000);
		ledOff();
		msleep(5000);
		startTrigger();
		printk(KERN_INFO "MoroseCode: debugging end\n");
	}

	delayLength = msecs_to_jiffies(1000);

	mutex_init(&moroseCode_mutex);
	printk(KERN_INFO "MoroseCode: mutex was initalized\n");

	return 0;
}

static void __exit moroseCode_exit(void) {
	del_timer(&timer);
	startTrigger();
    mutex_destroy(&moroseCode_mutex);
	device_destroy(moroseCodeClass, MKDEV(majorNumber, 0));
	class_unregister(moroseCodeClass);
	class_destroy(moroseCodeClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "MoroseCode: Goodbye from the LKM\n");
}

module_init(moroseCode_init);
module_exit(moroseCode_exit);

static int dev_open(struct inode *inode, struct file *file) {
	if (!mutex_trylock(&moroseCode_mutex)) {
		return -EBUSY;
	}

    printk(KERN_INFO "dev_open: successful\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
	mutex_unlock(&moroseCode_mutex);
    printk(KERN_INFO "dev_release: successful\n");
    return 0;
}

static ssize_t dev_write(struct file *file, const char *buf, size_t count, loff_t * ppos) {
	stopTrigger();
	newMessage = 1;
	memset(message, '\0', 256);
	messageLen = 0;

	if (count > 256) {
		while (count > 256) {
			--count;
		}
	}

	if (copy_from_user(message, buf, count)) {
		printk(KERN_INFO "dev_write: failed\n");
		return -1;
	}
	messageLen = count;

	printk(KERN_INFO "dev_write: accepting %d bytes\n", messageLen);
	printk(KERN_INFO "MoroseCode: word recieved %s of length %d\n", message, messageLen);

	messagePos = 0;
	translatedLetterPos = 0;
	translatedLetter = mcodestring((int)message[messagePos]);
	numSegments = strlen(translatedLetter);
	printk(KERN_INFO "MoroseCode: Current letter \"%c\" and pattern \"%s\" of size %d\n", message[messagePos], translatedLetter, numSegments);

	timer_setup(&timer, blinkPattern, 0);
	mod_timer(&timer, jiffies + delayLength);

    return 0;
}

static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ptr) {
    printk(KERN_INFO "dev_read: returning zero bytes\n");
    return 0;
}

static long dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg) {
    printk(KERN_INFO "hello_ioctl: cmd=%ld, arg=%ld\n", cmd, arg);
    return 0;
}

static int openFile(void) {
	mm_segment_t fs_saved = get_fs(); //saved current FS segment
	set_fs(KERNEL_DS); //set file segment to??

	fp = filp_open(USER_LED_PATH,  O_WRONLY | O_CREAT, 0444);

	if (!fp || IS_ERR(fp)) {
		set_fs(fs_saved);
		printk(KERN_INFO "MoroseCode: error openning user led 0 trigger file");
		return -EIO;
	}

	set_fs(fs_saved);
	printk(KERN_INFO "MoroseCode: successfully opened user led 0 trigger file");

	return 0;
}

static int closeFile(void) {
	mm_segment_t fs_saved = get_fs();
	set_fs(KERNEL_DS);

	if (fp) {
		filp_close(fp, NULL);
	}

	set_fs(fs_saved);

	printk(KERN_INFO "MoroseCode: successfully closed user led 0 trigger file");
	return 0;
}

ssize_t writeToFile(void *in, ssize_t inLen) {
	ssize_t written;
	loff_t pos = 0;
	mm_segment_t fs_saved = get_fs();
	set_fs(KERNEL_DS);

	written = vfs_write(fp, in, inLen, &pos);

	set_fs(fs_saved);
	return written;
}

static void stopTrigger(void) {
	int ret = openFile();

	ret = writeToFile("none", 4);

	closeFile();
}

static void startTrigger(void) {
	int ret = openFile();

	ret = writeToFile("heartbeat", 9);

	closeFile();
}

void ledOn(void) {
	printk(KERN_INFO "MoroseCode: turning on led\n");
	*gpioSetDataOutAddr = USR0;
}

void ledOff(void) {
	printk(KERN_INFO "MoroseCode: turning off led\n");
	*gpioClearDataOutAddr = USR0;
}

void blinkPattern(struct timer_list * data) {
	if (newMessage) {
		//Check if there is letters to process
		if (messagePos < messageLen) {
			//check if letter to process is a space between words
			if (message[messagePos] != ' '){
				if (isOn) {
					//if led if on - turn it off and increment the messagePos, translatedLetterPos, and translate new letter as needed
					ledOff();
					isOn = 0;
					if (translatedLetterPos + 1 < numSegments) {
						translatedLetterPos += 1;
						mod_timer(&timer, jiffies + delayLength); //1 segment delay between parts of same letter
					} else {
						printk(KERN_INFO "MoroseCode: updating current letter and pattern\n");
						translatedLetterPos = 0;
						messagePos += 1; //Checked the beginning so last letter should work out 
						if (messagePos < messageLen) {
							translatedLetter = mcodestring((int)message[messagePos]);
							numSegments = strlen(translatedLetter);
							printk(KERN_INFO "MoroseCode: Next letter \"%c\" and pattern \"%s\"\n", message[messagePos], translatedLetter);
						}
						mod_timer(&timer, jiffies + (delayLength * 3)); //3 segment delay between letters of same word

					}

					printk(KERN_INFO "MoroseCode: current messagePos %d\n", messagePos);
					printk(KERN_INFO "MoroseCode: current translatedLetterPos %d\n", translatedLetterPos);
				} else {
					ledOn();
					isOn = 1;
					switch(translatedLetter[translatedLetterPos]) {
						case '.':
							mod_timer(&timer, jiffies + delayLength);
							break;
						case '-':
							mod_timer(&timer, jiffies + (delayLength * 3));
							break;
					}
				}
			} else {
				//set timer for seven delays between words
				printk(KERN_INFO "MoroseCode: current letter is a space\n");
				messagePos += 1;
				translatedLetter = mcodestring((int)message[messagePos]);
				numSegments = strlen(translatedLetter);
				mod_timer(&timer, jiffies + (delayLength * 7));
			}
		} else {
			printk(KERN_INFO "MoroseCode: message finished\n");
			newMessage = 0;
			del_timer(&timer);
			startTrigger();
		}
	}
}