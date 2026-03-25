CC := cc
PKG_CONFIG := pkg-config

TARGET := mojave
BUILD_DIR := build

SOURCES := \
	src/main.c \
	src/runtime.c \
	src/content.c \
	flecs/distr/flecs.c

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

CPPFLAGS := \
	-D_POSIX_C_SOURCE=200809L \
	-DFLECS_CUSTOM_BUILD \
	-DFLECS_OS_API_IMPL \
	-I./src \
	-I./flecs/distr \
	$(shell $(PKG_CONFIG) --cflags raylib yyjson)

CFLAGS := -std=c99 -Wall -Wextra -pedantic
LDLIBS := $(shell $(PKG_CONFIG) --libs raylib yyjson) -lm

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
