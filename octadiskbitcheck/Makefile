# M.AKIMOTO
# 2025/05/26
# Makefile for octadiskbitcheck

CC = gcc
CFLAGS = -O2 -Wall
TARGET = octadiskbitcheck

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

install:
	cp $(TARGET) ~/bin

clean:
	rm -f $(TARGET)
