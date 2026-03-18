MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wno-unused-result -O3

SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagd.a

TARGET=

all: build

debug: TARGET = debug
debug: build

valgrind: debug
	make -C $(TEST_DIR) valgrind

build: tagd libtagd

tagd: force_look
	@printf '[build] %s\n' 'tagd/src'
	@make -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"
	@printf '[build] %s\n' 'tagd/src/domain'
	@make -C $(SRC_DIR)/domain $(TARGET) CXXFLAGS="$(CXXFLAGS)"

libtagd: tagd $(LIB)

$(LIB):
	@printf '[ar] %s\n' 'tagd/libtagd.a'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o $(SRC_DIR)/domain/*.o 

tests: force_look libtagd 
	@printf '[test] %s\n' 'tagd'
	@make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

# 'true' forces make to look (otherwise its always up to date)
force_look:
	@true

clean:
	rm -rf  $(BUILD_DIR)
	@make -C $(SRC_DIR) clean
	@make -C $(SRC_DIR)/domain clean
	@make -C $(TEST_DIR) clean
