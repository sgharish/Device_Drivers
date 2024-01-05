/*headers to initialise a device driver (related: init_module(), exit_module, module decelerations)*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h> // for character driver registeration
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>


#define TRIGGER         26
#define ECHO            6

/************************Global Variables*******************/
/*decelaring the driver device name using cdev character ref: book(functions explained) or linux doc*/
struct cdev *hcsr04_dev;
/*since we will be doing a dynamic allocation major is consider 0. Please check the list if you want to utilise existing onces*/
int hcsrmjr = 0;
int irq_num;
/*Initialise ktime*/
ktime_t etime_start, etime_end;
/*Initialise semaphores*/
struct semaphore IRQ_sem;
spinlock_t hcsr04_lock;

//interrupt handler for echo
irqreturn_t irq_echo_handler(int irq, void *dev_id){
         /*added timer based calculation to update the distance*/
        int echo_status =  gpio_get_value(ECHO);
        if(echo_status == 1){
                etime_start = ktime_get();
        }else if(echo_status == 0){
                etime_end = ktime_get();
                up(&IRQ_sem);
        }
        return IRQ_HANDLED;

}

/*Function pointer which is decleraded in the fops. This is to read the value from the sensor*/
ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
   /*outputed value in mm*/
    unsigned int range_mm;
     ktime_t level_time;
    sema_init(&IRQ_sem, 1);
    spin_lock_init(&hcsr04_lock);

 /*checking position of the pointer in the file. If its at the end of file close it or stop reading*/
    if (*f_pos > 0) {
        return 0; 
    }
    /*sending out trigger pulse for 10µs*/
    gpio_set_value(TRIGGER,1);
    udelay(10);
    gpio_set_value(TRIGGER,0);

/*wait for the echo to read*/
    down(&IRQ_sem);

    /*use ktime to measure the high level time*/
    level_time = ktime_sub(etime_start,etime_end);
    range_mm = ((unsigned int)ktime_to_us(level_time)) * (340/2000);
    
    /*store the read value into the variable . Similar to normal drivers. It is good to read with individual decelartions*/
    if (copy_to_user(buf, &range_mm, sizeof(range_mm))) {
        return -EFAULT;
    }
    /*move the pointer*/
    *f_pos += sizeof(range_mm);
    spin_unlock(&hcsr04_lock);
    pr_info("I am working");
    return sizeof(range_mm);
}

int hcsr04_open(struct inode *inode, struct file *filp){
        if(spin_is_locked(&hcsr04_lock)!=0){
                return -EBUSY;
        }
        return 0;
}


int hcsr04_release (struct inode *inode, struct file *filp ){
        spin_unlock(&hcsr04_lock);
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

        printk("Loading testing sensor: HC-SR04");

        err += alloc_chrdev_region(&dev, 0, 1, "distsensor");
        
        if(err != 0){
                pr_alert ("Failed to register");
                return err;
        }
         
        hcsr04_dev = cdev_alloc();
        hcsr04_dev->ops=&hcsr04_fops;
        err = cdev_add (hcsr04_dev,dev,1);
        err =0;
         /*can be used to valid if you are unsure. Not needed in this case*/
        err+= gpio_is_valid(TRIGGER);
        if(err==false){
                return -EINVAL;
        }
        /*Needed to initialise the type of GPIO and this need to be release*/
        err+= gpio_request(TRIGGER,"GPIO TRIGGER");
        /*setting this pin as output and pulldown*/
       
        gpio_direction_output(TRIGGER, 0);
        gpio_set_value(TRIGGER, 0);
        err = 0;
        err += gpio_is_valid(ECHO);
        if(err==false){
                return -EINVAL;
        }
        irq_num = gpio_to_irq(ECHO);
        // reference : https://tldp.org/LDP/lkmpg/2.6/html/x1256.html
        if (request_irq (irq_num,void* irq_echo_handler,
                          IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                         "distsensor",NULL)){
                printk("Error register interrupt");
                return -1;
        }
        return 0;
}

/*to unload the module and clean it api calls in the linux*/
void hcsr04_exit(void)
{       
        dev_t dev = MKDEV(hcsrmjr,0);
        cdev_del (hcsr04_dev);
        gpio_set_value(TRIGGER,0);
        gpio_free(TRIGGER);
        free_irq(irq_num);
        printk("HC-SR04 Going to sleep");
        unregister_chrdev_region(dev,1);
}

/*license related info*/
MODULE_AUTHOR("Harish Gopal");
MODULE_LICENSE("GPL");

/*module initialization and removing*/
module_init(hcsr04_init);
module_exit(hcsr04_exit);


