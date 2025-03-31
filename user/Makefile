CC = arm-linux-gnueabihf-gcc
INCLUDE_DIR=$(PWD)/../include
CFLAGS =-I$(INCLUDE_DIR) -Wall

all: dense_mm sort

clean:
	rm dense_mm
	rm sort

dense_mm: dense_mm.c
	$(CC) $(CFLAGS) dense_mm.c -o dense_mm

sort: sort.c
	$(CC) $(CFLAGS) sort.c -o sort
