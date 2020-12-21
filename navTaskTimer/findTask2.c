#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/list.h>
#include <linux/init_task.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shaneka Lewis");
MODULE_DESCRIPTION("Linux driver to locate specified process with timer support");
MODULE_VERSION("0.1");

static char *name;
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "Process to be searched for by name.");

struct task_struct *task;
struct task_struct *child;
struct list_head *list;
int found = 0;

int timerFlag = 0;
unsigned long tDelay;

/*struct timer_list {         
	unsigned long expires;            
	unsigned long data;               
	void (*function)(unsigned long);              
};*/

static struct timer_list timer;
//timer.expires
//struct timer_list TIMER_INITIALIZER(searchForTask, 0, 0);
//DEFINE_TIMER(timer, searchForTask, 0, 0);
//void init_timer(struct timer_list *timer)
//void add_timer(struct timer_list *timer);
//int del_timer(struct timer_list *timer);

/*Notes: Needed functions are timer_setup(ptr to timer, func, arg for func),
add_timer(ptr to timer), mod_timer(ptr to timer, new expiration), del_timer(ptr to timer)*/

void searchForTask(struct timer_list * data) {
	printk(KERN_INFO "findTask: Locating task \"%s\"...\n", name);
		
	task = &init_task;
	child = task;
	
	for_each_process(task) {
		list_for_each(list, &task->children) {
			child = list_entry(list, struct task_struct, sibling);
			if (strcmp(child->comm, name) == 0) {
				printk(KERN_INFO "findTask: Found %s with pid # %d\n", name, child->pid);
				found = 1;
				printk(KERN_INFO "findTask: Stopping timer\n");
				del_timer(&timer);
				timerFlag = 0;
			}
		}
	}
	
	if (found == 0) {
		printk(KERN_INFO "findTask: Not Found %s\n", name);
		mod_timer(&timer, jiffies + tDelay); 
	}
}

static int findTask_init(void){
	printk(KERN_INFO "Loading driver findTask...");
	
	if (name == NULL) {    
		printk(KERN_INFO "findTask Error: no process name specified\n");  
	} else {
		printk(KERN_INFO "findTask: Loaded succesfully\n");
		
		tDelay = msecs_to_jiffies(5000); //about 5 seconds-ish
		timer_setup(&timer, searchForTask, 0);
		printk(KERN_INFO "findTask: Staring timer\n");
		add_timer(&timer);
		//mod_timer(&timer, jiffies + tDelay); 
		timerFlag = 1;
	}
	
	return 0;
}

static void findTask_exit(void){
	if (timerFlag == 1) {
		printk(KERN_INFO "findTask: Stopping timer\n");
		del_timer(&timer);
	}
	 
	printk(KERN_INFO "Removing driver findTask\n");
}
	
module_init(findTask_init);
module_exit(findTask_exit);
