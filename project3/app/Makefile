CC := arm-none-linux-gnueabi-gcc
target = app
objects = app.c

all: $(target)

$(target):$(objects)
	$(CC) -static -o $(target) $(objects)

clean : 
	rm -rf app