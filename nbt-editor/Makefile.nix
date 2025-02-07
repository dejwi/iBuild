# Makefile for compiling a shared library (.so) for MacOS and Linux 

CC = clang
CFLAGS = -I. -fPIC -DZF_LOG_DEF_LEVEL=ZF_LOG_VERBOSE
LDFLAGS = -shared -lz

BUILD_DIR = build

DEPS = mc.h
# Exclude main.c from the list of source files.
SRC = $(filter-out main.c, $(wildcard *.c))
OBJ = $(SRC:%.c=$(BUILD_DIR)/%.o)

# Default target builds the shared library with a .so extension.
all: $(BUILD_DIR)/libmc.so

# Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile each .c file into an object file in the build directory
$(BUILD_DIR)/%.o: %.c $(DEPS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Link all object files into the shared library with a .so extension.
$(BUILD_DIR)/libmc.so: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Clean command
clean:
	rm -rf $(BUILD_DIR)
