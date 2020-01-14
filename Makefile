CC = g++
CFLAGS = -g -Wall
SRCS = main.cpp x-touch.cpp
PROG = x-touch-test

$(PROG):$(SRCS) Makefile
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS)

