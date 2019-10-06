SRC_DIR = ./src
BUILD_DIR = ./bin
LIB_DIR = ./lib
TEST_DIR = ./tests
LIB = $(LIB_DIR)/libtagsh.a

TARGET=
debug: TARGET = debug
debug: all

valgrind: debug
	make -C $(TEST_DIR) valgrind

all: build

build: tagsh $(LIB)

$(LIB):
	[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	ar rcs $(LIB) src/*.o

tagsh: force_look
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	make -C	$(SRC_DIR) $(TARGET)

tests: force_look build
	make -C $(TEST_DIR) $(TARGET)

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR) $(LIB_DIR)
	make -C $(SRC_DIR) clean

