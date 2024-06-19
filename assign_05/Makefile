
obj-m = gpio-intr.o

all:
	make -C /lib/modules/4.19.94-ti-r42/build M=`pwd` modules

clean:
	make -C /lib/modules/4.19.94-ti-r42/build M=`pwd` clean

.PHONY: all clean

