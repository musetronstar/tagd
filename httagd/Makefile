SRC_DIR = ./src
BUILD_DIR = ./bin
TEST_DIR = ./tests

all: build

DEBUG=
debug: DEBUG=debug
debug: build

build: tagsh

tagsh: force_look
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	make -C	$(SRC_DIR) $(DEBUG)

tests: force_look tagsh
	make -C $(TEST_DIR) $(DEBUG)

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR)
	make -C $(SRC_DIR) clean
