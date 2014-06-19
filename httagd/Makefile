SRC_DIR = ./src
BIN_DIR = ./bin
LIB_DIR = ./lib
TEST_DIR = ./tests

LIB = $(LIB_DIR)/libhttagd.a

all: build

DEBUG=
debug: DEBUG=debug
debug: build

build: src lib

src: force_look
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	make -C	$(SRC_DIR) $(DEBUG)

tests: force_look lib
	make -C $(TEST_DIR) $(DEBUG)

lib: src $(LIB)

$(LIB):
	[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o

force_look:
	true

clean:
	rm -rf  $(LIB_DIR) $(BIN_DIR)
	make -C $(SRC_DIR) clean
