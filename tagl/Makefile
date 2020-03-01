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
	make -C	$(SRC_DIR) $(TARGET)

$(LIB):
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o

tests: libtagl force_look 
	make -C $(TEST_DIR) $(TARGET)

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR)
	make -C $(SRC_DIR) clean
	make -C $(TEST_DIR) clean

