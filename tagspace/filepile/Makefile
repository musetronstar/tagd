MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libfilepile.a

TARGET=

all: build

debug: TARGET = debug
debug: build

build: filepile $(LIB)

filepile: force_look
	@printf '[build] %s\n' 'filepile/src'
	@$(MAKE) -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

$(LIB): filepile
	@printf '[ar] %s\n' 'filepile/libfilepile.a'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o

# Store tests exercise the public tagspace archive, including both private modules.
tests: force_look
	@$(MAKE) -C .. libtagspace CXXFLAGS="$(CXXFLAGS)"
	@printf '[test] %s\n' 'filepile'
	@$(MAKE) -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

force_look:
	@true

clean:
	@$(MAKE) -C $(SRC_DIR) clean
	@$(MAKE) -C $(TEST_DIR) clean
	rm -rf $(BUILD_DIR)
