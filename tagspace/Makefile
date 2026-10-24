include backend.mk

MAKEFLAGS += --no-print-directory
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests
FILEPILE_DIR = ./filepile
LIB = $(BUILD_DIR)/libtagspace.a
# Only current sources contribute objects after a file moves into a private module.
OBJS = $(patsubst %.cc,%.o,$(wildcard $(SRC_DIR)/*.cc))

TARGET=

all: build

debug: TARGET = debug
debug: build

build: libtagspace

tagspace: backend filepile force_look
	@printf '[build] %s\n' 'tagspace/src'
	@make -C $(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

libtagspace: tagspace $(LIB)

# Bundle the private backend and filepile so consumers link only the public module archive.
$(LIB): tagspace
	@printf '[ar] %s\n' 'tagspace/libtagspace.a'
	@[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(OBJS) $(FILEPILE_DIR)/src/*.o tagdb/src/*.o tagdb/$(TAGDB_BACKEND)/src/*.o

tests: force_look libtagspace
	@make -C tagdb tests CXXFLAGS="$(CXXFLAGS)"
	@printf '[test] %s\n' 'filepile'
	@make -C $(FILEPILE_DIR)/tests $(TARGET) CXXFLAGS="$(CXXFLAGS)"
	@printf '[test] %s\n' 'tagspace'
	@make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

# 'true' forces make to look (otherwise its always up to date)
force_look:
	@true

backend: force_look
	@make -C tagdb $(TARGET) CXXFLAGS="$(CXXFLAGS)"

filepile: force_look
	@make -C $(FILEPILE_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

clean:
	@make -C tagdb clean
	@make -C $(FILEPILE_DIR) clean
	rm -rf  $(BUILD_DIR)
	@make -C $(SRC_DIR) clean
	@make -C $(TEST_DIR) clean
