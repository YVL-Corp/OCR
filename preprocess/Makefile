TARGET = main
CC = gcc

CFLAGS = -Wall -Wextra $(shell pkg-config --cflags gtk+-3.0) -g

LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -rdynamic -lm

SRCS = main.c processing.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)
