SRC_DIR = ./src
PRG_DIR = ./sqlite
BUILD_DIR = ./lib
TEST_DIR = ./tests
LIB = $(BUILD_DIR)/libtagdb-sqlite.a

all: build

DEBUG=
debug: DEBUG=debug
debug: build

build: tagdb $(LIB)

tagdb: force_look
	make -C $(SRC_DIR) $(DEBUG)
	make -C $(PRG_DIR) $(DEBUG)

$(LIB):
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o $(PRG_DIR)/src/*.o

tests: tagdb $(LIB) force_look
	make -C $(TEST_DIR) $(DEBUG)

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

clean:
	make -C $(SRC_DIR) clean
	make -C $(PRG_DIR) clean
	make -C $(TEST_DIR) clean
	rm -rf  $(BUILD_DIR)
