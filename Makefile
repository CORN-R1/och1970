#
# Makefile for och1970 sensor driver.
#
ccflags-y += -Wno-unused-result
ccflags-y += -Wno-unused-function
obj-$(CONFIG_SENSORS_OCH1970) += och1970.o

#ccflags-y += -Wno-unused-result
#ccflags-y += -Wno-unused-function
#obj-m :=och1970.o
#LINUX_KERNEL_PATH :=/home/powersys/lxx/HSC_8541E_ALL_F1/idh.code/out/target/product/s
#CROSS_COMPILE_PATH :=/home/powersys/lxx/HSC_8541E_ALL_F1/idh.code/prebuilts/gcc/linu
#PWD :=$(shell pwd)
#all:
# make ARCH=arm64 -C $(LINUX_KERNEL_PATH) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE_PATH)
#clean:
#  make ARCH=arm64 -C $(LINUX_KERNEL_PATH) M=$(PWD) clean
