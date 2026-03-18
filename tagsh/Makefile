MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wno-unused-result -O3

SRC_DIR = ./src
BUILD_DIR = ./bin
LIB_DIR = ./lib
TEST_DIR = ./tests
LIB = $(LIB_DIR)/libtagsh.a

all: build

TARGET=
debug: TARGET = debug
debug: all

valgrind: debug
	make -C $(TEST_DIR) valgrind

build: tagsh $(LIB)

$(LIB):
	@printf '[ar] %s\n' 'tagsh/libtagsh.a'
	@[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	ar rcs $(LIB) src/*.o

tagsh: force_look
	@printf '[build] %s\n' 'tagsh/src'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	@make -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

tests: force_look build
	@printf '[test] %s\n' 'tagsh'
	@make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

force_look:
	@true

clean:
	rm -rf  $(BUILD_DIR) $(LIB_DIR)
	@make -C $(SRC_DIR) clean
