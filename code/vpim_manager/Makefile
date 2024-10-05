CC = gcc
CFLAGS = -Wall -Wextra -Werror -I include 
LDFLAGS = -lzmq -lpthread
#LDLIBS = -L/usr/lib/x86_64-linux-gnu/libzmq.a -lzmq
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TARGET = $(BIN_DIR)/app
MAIN_FILE = ./main.c

SRC_FILES = $(wildcard $(SRC_DIR)/*.c) $(MAIN_FILE)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	@mkdir -p $(@D)
	$(CC)  $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)