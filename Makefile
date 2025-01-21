CC = clang
CFLAGS = -I.
LDFLAGS = -lz

BUILD_DIR = build

DEPS = mc.h zlib-utils.h
SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=$(BUILD_DIR)/%.o)

# Default target
all: $(BUILD_DIR)/main

# Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Pattern rule for building object files in the build directory
$(BUILD_DIR)/%.o: %.c $(DEPS) | $(BUILD_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

# Build the main executable in the build directory
$(BUILD_DIR)/main: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# Clean command
clean:
	rm -rf $(BUILD_DIR)
