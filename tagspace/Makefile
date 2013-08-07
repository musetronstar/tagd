SRC_DIR = ./src
PRG_DIR = ./sqlite
BUILD_DIR = ./build
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagspace-sqlite.a

all: tagspace $(LIB)

tagspace: force_look
	make -C $(SRC_DIR)
	make -C $(PRG_DIR)

$(LIB):
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o $(PRG_DIR)/src/*.o

tests: tagspace $(LIB) force_look
	make -C $(TEST_DIR)

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

clean:
	make -C $(SRC_DIR) clean
	make -C $(PRG_DIR) clean
	make -C $(TEST_DIR) clean
	rm -rf  $(BUILD_DIR)
