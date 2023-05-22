obj-m += pswap.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

insmod:
	sudo insmod pswap.ko pid=$(shell pidof example) addr=93824992235520 rip=93824992236045
	sudo dmesg -wH

rmmod:
	sudo rmmod pswap.ko
	sudo dmesg -C
