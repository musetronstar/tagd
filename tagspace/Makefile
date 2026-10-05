MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagspace.a

TARGET=

all: build

debug: TARGET = debug
debug: build

build: libtagspace

tagspace: backend force_look
	@printf '[build] %s\n' 'tagspace/src'
	@make -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

libtagspace: tagspace $(LIB)

# Bundle the private backend so consumers link only the public module archive.
$(LIB): tagspace
	@printf '[ar] %s\n' 'tagspace/libtagspace.a'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o tagdb/src/*.o tagdb/sqlite/src/*.o

tests: force_look libtagspace
	@make -C tagdb tests CXXFLAGS="$(CXXFLAGS)"
	@printf '[test] %s\n' 'tagspace'
	@make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

# 'true' forces make to look (otherwise its always up to date)
force_look:
	@true

backend: force_look
	@make -C tagdb $(TARGET) CXXFLAGS="$(CXXFLAGS)"

clean:
	@make -C tagdb clean
	rm -rf  $(BUILD_DIR)
	@make -C $(SRC_DIR) clean
	@make -C $(TEST_DIR) clean
