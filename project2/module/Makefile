#Makefile for a basic kernel module

obj-m   := module.o
CC := arm-none-linux-gnueabi-gcc
KDIR    :=/work/achroimx_kernel
PWD     :=$(shell pwd)

all: driver

driver:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules ARCH=arm

clean:
	rm -rf *.ko
	rm -rf *.mod.c
	rm -rf *.o
	rm -rf *.symvers
	rm -rf *.order

