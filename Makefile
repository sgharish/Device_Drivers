obj-m := hc-sr04.o
lib-y := sem lpthread

all:
        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
        make -C /lib/modules/$(shell unmae -r)/build M=$(PWD) clean
