EXE = extraction
SRCS = main.c extraction.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -g -Wall -Wextra `pkg-config --cflags gdk-pixbuf-2.0`
LDFLAGS = `pkg-config --libs gdk-pixbuf-2.0`

all: $(EXE)

$(EXE): $(OBJS)
	gcc $(CFLAGS) -o $(EXE) $(OBJS) $(LDFLAGS)

%.o: %.c extraction.h
	gcc $(CFLAGS) -c $< -o $@

run: all
	./$(EXE) image_corrigee.png

clean:
	rm -f $(EXE) $(OBJS)
