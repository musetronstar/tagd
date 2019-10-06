SRC_DIR = ./src
PRG_DIR = ./sqlite
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagdb-sqlite.a

TARGET=

debug: TARGET = debug
debug: all

valgrind: debug
	make -C $(TEST_DIR) valgrind

all: build

build: tagdb $(LIB)

tagdb: force_look
	make -C $(SRC_DIR) $(TARGET)
	make -C $(PRG_DIR) $(TARGET)

$(LIB):
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o $(PRG_DIR)/src/*.o

tests: tagdb $(LIB) force_look
	make -C $(TEST_DIR) $(TARGET)

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

clean:
	make -C $(SRC_DIR) clean
	make -C $(PRG_DIR) clean
	make -C $(TEST_DIR) clean
	rm -rf  $(BUILD_DIR)
