CC = clang
CFLAGS = -Wall -Wextra -g -std=c11 -pedantic
LDFLAGS =

# Target binary
TARGET = server

# Source files
SRCS = server.c mqtt.c sockets.c management.c
OBJS = $(SRCS:.c=.o)

# Header files for dependency tracking
HEADERS = mqtt.h sockets.h errors.h management.h

# Default target
all: $(TARGET)

# Link the target binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -f $(OBJS) $(TARGET)

# Run the server
run: $(TARGET)
	./$(TARGET)

# Mark targets that don't represent files
.PHONY: all clean run install
