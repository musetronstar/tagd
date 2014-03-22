SRC_DIR = ./src
BUILD_DIR = ./lib
TEST_DIR = ./tests

LIB = $(BUILD_DIR)/libtagl.a

DEBUG=
debug: DEBUG=debug
debug: build

all: build

build: libtagl

libtagl: tagl $(LIB)

tagl: force_look
	make -C	$(SRC_DIR) $(DEBUG)

$(LIB):
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o

tests: libtagl force_look 
	make -C $(TEST_DIR) $(DEBUG)

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR)
	make -C $(SRC_DIR) clean
	make -C $(TEST_DIR) clean

