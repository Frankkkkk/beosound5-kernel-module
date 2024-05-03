obj-m += beosound.o

PWD := $(CURDIR)
CUR := $(shell uname -r)

all:
	make -C /lib/modules/$(CUR)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(CUR)/build M=$(PWD) clean
