CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic
LDFLAGS = -lpthread

# List all your source files here
SOURCES = httpserver.c queue.c net_utils.c
HEADERS = queue.h net_utils.h
OBJECTS = $(SOURCES:.c=.o)
TARGET = httpserver

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJECTS)

format:
	clang-format -i -style=file *.[ch]

.PHONY: all clean format
