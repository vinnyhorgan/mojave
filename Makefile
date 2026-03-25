CC := cc
CFLAGS := -std=c99 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L -DFLECS_CUSTOM_BUILD -DFLECS_OS_API_IMPL -I./src -I./flecs/distr $(shell pkg-config --cflags raylib yyjson)
LDFLAGS := $(shell pkg-config --libs raylib yyjson) -lm

TARGET := mojave
SOURCES := src/main.c src/runtime.c src/content.c flecs/distr/flecs.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
