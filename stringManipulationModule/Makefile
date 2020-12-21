obj-m+=testchar.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(shell pwd) modules
	gcc -o main1 main1.c
	gcc -o main2 main2.c
	
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(shell pwd) clean
	rm main1 main2
