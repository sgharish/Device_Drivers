/*headers to initialise a device driver (related: init_module(), exit_module, module decelerations)*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h> // for character driver registeration
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kdev_t.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/timekeeping.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#define TRIGGER         26
#define ECHO            6
#define ECHO_TIMEOUT    1000

/************************Global Variables*******************/
/*decelaring the driver device name using cdev character ref: book(functions explained) or linux doc*/
struct cdev *hcsr04_dev;
/*since we will be doing a dynamic allocation major is consider 0. Please check the list if you want to utilise exist                                                                                                                        ing onces*/
int hcsrmjr = 0;
int irq_num;
/*Initialise ktime*/
ktime_t etime_start, etime_end;
/*Initialise semaphores*/
struct semaphore IRQ_sem;
spinlock_t hcsr04_lock;
wait_queue_head_t short_queue;
int condition;

//interrupt handler for echo
irqreturn_t irq_echo_handler(int irq, void *dev_id){
        /*added timer based calculation to update the distance*/
        int echo_status = gpio_get_value(ECHO);
        if(echo_status == 1){
                printk(KERN_INFO"Starting the wait");
                etime_start = ktime_get();
        }else{
                printk(KERN_INFO"wait complete");
                etime_end = ktime_get();
                condition = 1;
                wake_up_interruptible(&short_queue);
        }
        return IRQ_HANDLED;
}
/*Function pointer which is decleraded in the fops. This is to read the value from the sensor*/
ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
   /*outputed value in mm*/
    unsigned int range_mm = 40;
    ktime_t level_time;
    long int echo_time_out;
    condition =0;
    //spin_lock_init(&hcsr04_lock);

   /*checking position of the pointer in the file. If its at the end of file close it or stop reading*/
    if (*f_pos > 0) {
        return 0;
    }
    spin_lock(&hcsr04_lock);
   /*sending out trigger pulse for 10µs*/
    gpio_set_value(TRIGGER,1);
    udelay(10);
    gpio_set_value(TRIGGER,0);

    /*wait for the echo to read*/
   echo_time_out  = wait_event_interruptible_timeout(short_queue, condition, ECHO_TIMEOUT);// waiting for the interrupt to complete
 // on timeout let the user and kernel know
    if (echo_time_out == 0){
        pr_warn("Timeout Error");
        spin_unlock(&hcsr04_lock);
        return -ETIME;
    }

    /*use ktime to measure the high level time*/
    level_time = ktime_sub(etime_start,etime_end);
    range_mm = ((unsigned int)ktime_to_us(level_time)) * (340/2000);
    /*store the read value into the variable . Similar to normal drivers. It is good to read with individual decelart                                                                                                                        ions*/
    if (copy_to_user(buf, &range_mm, sizeof(range_mm))) {
        spin_unlock(&hcsr04_lock);
        return -EAGAIN;
    }
    /*move the pointer*/
    *f_pos += sizeof(range_mm);
    spin_unlock(&hcsr04_lock);
    pr_info(KERN_INFO"I am working");
    return sizeof(range_mm);
}

int hcsr04_open(struct inode *inode, struct file *filp){
        /*if(spin_is_locked(&hcsr04_lock)!=0){
                return -EBUSY;
        }*/
        return 0;
}


int hcsr04_release (struct inode *inode, struct file *filp ){
        //spin_unlock(&hcsr04_lock);
        return 0;
}

/*decelaring the fops structure for the drive this is common for all the drivers*/
struct file_operations hcsr04_fops = {
        .owner   = THIS_MODULE, //owner mention
        .read    = hcsr04_read, // function pointer for read
        .open    = hcsr04_open, // function pointer to avoid concurrancy and race condition
        .release = hcsr04_release // function pointer for release
};

/*to load the module and initialise it*/
int hcsr04_init(void)
{
        int err = 0;
        dev_t dev = MKDEV(hcsrmjr,0);
        spin_lock_init(&hcsr04_lock);
        init_waitqueue_head(&short_queue);

        printk(KERN_DEBUG"Loading testing sensor: HC-SR04");

        err += alloc_chrdev_region(&dev, 0, 1, "distsensor");
        if(err != 0){
                pr_alert (KERN_WARNING"Failed to register");
                return err;
        }

        hcsrmjr = MAJOR(dev);
        hcsr04_dev = cdev_alloc();
        hcsr04_dev->ops=&hcsr04_fops;
        err = cdev_add (hcsr04_dev,dev,1);

        /*can be used to valid if you are unsure. Not needed in this case*/
        err+= gpio_is_valid(TRIGGER);
        /*Needed to initialise the type of GPIO and this need to be released or removed by free*/
        /*setting this pin as output and pulldown*/
        if(err == false){
                pr_alert("Error Trigger");
                return -EINVAL;
        }
        err+= gpio_request(TRIGGER,"GPIO TRIGGER");
        gpio_direction_output(TRIGGER, 0);
        gpio_set_value(TRIGGER, 0);
        err = 0;
        err += gpio_is_valid(ECHO);
        gpio_request(ECHO,"GPIO ECHO");
        gpio_direction_input(ECHO);
        if(err==false){
                pr_alert("Error Echo");
                return -EINVAL;
        }
        irq_num = gpio_to_irq(ECHO);
        if (request_irq (irq_num,(void *)irq_echo_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "distsensor", NULL)){
                printk("Error interrupt broke rising edge");
                return -1;
        }

        return 0;
}

/*to unload the module and clean it api calls in the linux*/
void hcsr04_exit(void)
{
        dev_t dev = MKDEV(hcsrmjr,0);
        cdev_del (hcsr04_dev);
        unregister_chrdev_region(dev,1);
        gpio_set_value(TRIGGER,0);
        gpio_free(TRIGGER);
        free_irq(irq_num,NULL);
        gpio_free(ECHO);
        printk("HC-SR04 Going to sleep");
}

/*license related info*/
MODULE_AUTHOR("Harish Gopal");
MODULE_LICENSE("GPL");

/*module initialisation and removing*/
module_init(hcsr04_init);
module_exit(hcsr04_exit);
