EXE = extraction
SRCS = main.c extraction.c image_export.c
OBJS = $(SRCS:.c=.o)

# Flags for compiler and linker (using gdk-pixbuf)
CFLAGS = -g -Wall -Wextra `pkg-config --cflags gdk-pixbuf-2.0`
LDFLAGS = `pkg-config --libs gdk-pixbuf-2.0`

all: $(EXE)

$(EXE): $(OBJS)
	gcc $(CFLAGS) -o $(EXE) $(OBJS) $(LDFLAGS)

# Rule to compile .c to .o
# Dependencies: .c file and relevant headers
%.o: %.c extraction.h image_export.h
	gcc $(CFLAGS) -c $< -o $@

run: all
	./$(EXE) Images/preprocessed_image_test_1.png

clean:
	rm -f $(EXE) $(OBJS)
	# Optional: remove output folder on clean
	rm -rf output
