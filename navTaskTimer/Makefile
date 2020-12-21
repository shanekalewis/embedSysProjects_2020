#Goal definition - specifies the module to be built
#obj-m defines a loadable module (m)
#obj-y defines a built in object (y)
obj-m+=findTask2.o

all:
	#shell uname -r returns current kernel build
	#-C switches dir to kernel dir
	#the M=$(PWD) specifies curr dir where project files exist
	make -C /lib/modules/$(shell uname -r)/build/ M=$(shell pwd) modules
	gcc -Wall -o bork bork.c
	
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(shell pwd) clean
	rm bork
