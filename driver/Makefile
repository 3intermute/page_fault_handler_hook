obj-m += pswap.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

insmod:
	sudo insmod driver.ko pid=$(shell pidof example) addr=93824992235913
	sudo dmesg -wH

rmmod:
	sudo rmmod driver.ko
	sudo dmesg -C
