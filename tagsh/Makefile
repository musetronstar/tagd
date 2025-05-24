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
	[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	ar rcs $(LIB) src/*.o

tagsh: force_look
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	make -C	$(SRC_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

tests: force_look build
	make -C $(TEST_DIR) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR) $(LIB_DIR)
	make -C $(SRC_DIR) clean

