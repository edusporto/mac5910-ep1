CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS =

# Target binary
TARGET = server

# Source files
SRCS = server.c mqtt.c utils.c
OBJS = $(SRCS:.c=.o)

# Header files for dependency tracking
HEADERS = mqtt.h utils.h errors.h

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
