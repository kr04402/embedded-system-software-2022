CC = arm-none-linux-gnueabi-gcc
target = 20161564.out
objects = main.c

all: $(target)

$(target):$(objects)
	$(CC) -static -o $@ $(objects)

clean:
	rm $(target)
