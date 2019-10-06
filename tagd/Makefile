SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagd.a

TARGET=

debug: TARGET = debug
debug: build

valgrind: debug
	make -C $(TEST_DIR) valgrind

all: build

build: tagd libtagd

tagd: force_look
	make -C	$(SRC_DIR) $(TARGET)
	make -C	$(SRC_DIR)/domain $(TARGET)

libtagd: tagd $(LIB)

$(LIB):
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o $(SRC_DIR)/domain/*.o 

tests: force_look libtagd 
	make -C $(TEST_DIR) $(TARGET)

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

clean:
	rm -rf  $(BUILD_DIR)
	make -C $(SRC_DIR) clean
	make -C $(SRC_DIR)/domain clean
	make -C $(TEST_DIR) clean

