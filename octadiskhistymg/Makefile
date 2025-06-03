# M.AKIMOTO
# 2025/05/26
# Makefile for octadiskhistymg 

CC = gcc
CFLAGS = -O2 -Wall -std=c99
TARGET = octadiskhistymg

.PHONY: all clean

all: $(TARGET)

#
# TARGET1
#
$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

install:
	cp $(TARGET) ~/bin

clean:
	rm -f $(TARGET) 

