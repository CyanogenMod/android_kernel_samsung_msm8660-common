#
# Makefile for miscellaneous I2C chip drivers.
#
# Do not add new drivers to this directory! It is DEPRECATED.
#
# Device drivers are better grouped according to the functionality they
# implement rather than to the bus they are connected to. In particular:
# * Hardware monitoring chip drivers go to drivers/hwmon
# * RTC chip drivers go to drivers/rtc
# * I/O expander drivers go to drivers/gpio
#

ifdef CONFIG_USA_MODEL_SGH_I717
obj-$(CONFIG_SENSORS_YDA165)	+= yda165_integ.o
else ifdef CONFIG_USA_MODEL_SGH_I577
obj-$(CONFIG_SENSORS_YDA165)	+= yda165_integ.o
else
obj-$(CONFIG_SENSORS_YDA165)    += yda165.o
endif

obj-$(CONFIG_SENSORS_CORE)		+= sensors_core.o

obj-$(CONFIG_OPTICAL_GP2A)		+= gp2a_proximity.o gp2a_light.o gp2a_proc.o
obj-$(CONFIG_OPTICAL_GP2AP020A00F)		+= gp2ap020_proximity.o gp2ap020_light.o

obj-$(CONFIG_USB_SWITCH_FSA9480)	+= fsa9480.o

obj-$(CONFIG_OPTICAL_TAOS)		+= taos.o
obj-$(CONFIG_GYRO_K3G)		+= k3g.o

obj-$(CONFIG_SENSORS_K3DH)		+= orientation.o

ifeq ($(CONFIG_I2C_DEBUG_CHIP),y)
EXTRA_CFLAGS += -DDEBUG
endif

#debug setting
ifeq ($(CONFIG_SENSORS_CORE),y)
EXTRA_CFLAGS += -DSENSORS_LOG_DUMP
endif