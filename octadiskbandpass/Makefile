# M.AKIMOTO
# 2025/05/26
# Makefile for octadiskbandpass

CC = gcc
CFLAGS = -O2 -Wall
TARGET = octadiskbandpass

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c -lfftw3 -lm 

install:
	cp $(TARGET) ~/bin

clean:
	rm -f $(TARGET) 
