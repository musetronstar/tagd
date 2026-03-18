MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wno-unused-result -O3

SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagl.a

TARGET=

all: build

debug: TARGET = debug
debug: all

valgrind: debug
valgrind:
	make -C $(TEST_DIR) valgrind

build: libtagl

libtagl: tagl $(LIB)

tagl: force_look
	@printf '[build] %s\n' 'tagl/src'
	@make -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

$(LIB):
	@printf '[ar] %s\n' 'tagl/libtagl.a'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o

tests: libtagl force_look 
	@printf '[test] %s\n' 'tagl'
	@make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

force_look:
	@true

clean:
	rm -rf  $(BUILD_DIR)
	@make -C $(SRC_DIR) clean
	@make -C $(TEST_DIR) clean
