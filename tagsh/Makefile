SRC_DIR = ./src
BUILD_DIR = ./bin
LIB_DIR = ./lib
TEST_DIR = ./tests
LIB = $(LIB_DIR)/libtagsh.a

all: build

DEBUG=
debug: DEBUG=debug
debug: build

build: tagsh $(LIB)

$(LIB):
	[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	ar rcs $(LIB) src/*.o

tagsh: force_look
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	make -C	$(SRC_DIR) $(DEBUG)

tests: force_look build
	make -C $(TEST_DIR) $(DEBUG)

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR) $(LIB_DIR)
	make -C $(SRC_DIR) clean
	# make -C $(TEST_DIR) clean # TODO

