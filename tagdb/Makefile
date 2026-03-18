MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wno-unused-result -O3

SRC_DIR = ./src
PRG_DIR = ./sqlite
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagdb-sqlite.a

TARGET=

all: build

debug: TARGET = debug
debug: all

valgrind: debug
	make -C $(TEST_DIR) valgrind

build: tagdb $(LIB)

tagdb: force_look
	@printf '[build] %s\n' 'tagdb/src'
	@make -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"
	@printf '[build] %s\n' 'tagdb/sqlite'
	@make -C $(PRG_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

$(LIB):
	@printf '[ar] %s\n' 'tagdb/libtagdb-sqlite.a'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o $(PRG_DIR)/src/*.o

tests: tagdb $(LIB) force_look
	@printf '[test] %s\n' 'tagdb'
	@make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

# 'true' forces make to look (otherwise its always up to date)
force_look:
	@true

clean:
	@make -C $(SRC_DIR) clean
	@make -C $(PRG_DIR) clean
	@make -C $(TEST_DIR) clean
	rm -rf  $(BUILD_DIR)
