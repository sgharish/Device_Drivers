/*headers to initialise a device driver (related: init_module(), exit_module, module decelerations)*/
#include <linux/init.h>
#include <linux/module.h>
/*headers to initialise the char driver MKDEV, alloc_chrdev_reg, del, add/init, */
#include <linux/cdev.h>
#include <linux/fs.h>
/*this for gpio*/
#include <linux/gpio.h>
#include <linux/delay.h>

#define TRIGGER         26
#define ECHO            6

/*decelaring the driver device name using cdev character ref: book(functions explained) or linux doc*/
struct cdev *hcsr04_dev;
/*since we will be doing a dynamic allocation major is consider 0. Please check the list if you want to utilise existing onces*/
int hcsrmjr = 0;
int irq_num;
//interrupt handler for echo
irqreturn_t irq_echo_handler(int irq, void *dev_id){
        /*added timer based calculation to update the distance*/
        returm IRQ_HANDLED;
}

/*Function pointer which is decleraded in the fops. This is to read the value from the sensor*/
ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
   /*outputed value in mm*/
    unsigned int range_mm;
    
    /*sending out trigger pulse for 10µs*/
    gpio_set_value(TRIGGER,1);
    udelay(10);
    gpio_set_value(TRIGGER,0);

    /*checking position of the pointer in the file. If its at the end of file close it or stop reading*/
    if (*f_pos > 0) {
        return 0; 
    }
    
    /*store the read value into the variable . Similar to normal drivers. It is good to read with individual decelartions*/
    if (copy_to_user(buf, &range_mm, sizeof(range_mm))) {
        return -EFAULT;
    }
    /*move the pointer*/
    *f_pos += sizeof(range_mm);
    return sizeof(range_mm);
}

/*decelaring the fops structure for the drive this is common for all the drivers*/
struct file_operations hcsr04_fops = {
        .owner   = THIS_MODULE, //owner mention
        .read    = hcsr04_read, // function pointer for read
        //.write         = hcsr04_write, // function pointer for write
        //.release = hcsr04_release // function pointer for release 
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
        /*Needed to initialise the type of GPIO and this need to be release$
        err+= gpio_request(TRIGGER,"GPIO TRIGGER");
        /*setting this pin as output and pulldown*/
        if(err!=0){
                return -EINVAL;
        }
        gpio_direction_output(TRIGGER, 0);
        gpio_set_value(TRIGGER, 0);
        err = 0;
        err += gpio_is_valid(ECHO);
        if(err!=0){
                return -EINVAL;
        }
        irq_num = gpio_to_irq(ECHO);
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


