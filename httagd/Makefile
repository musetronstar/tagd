SRC_DIR = ./src
BUILD_DIR = ./bin
TEST_DIR = ./tests

all: tagsh

tagsh: force_look
	[ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)
	make -C	$(SRC_DIR)

tests: force_look tagsh
	make -C $(TEST_DIR)

force_look:
	true

clean:
	rm -rf  $(BUILD_DIR)
	make -C $(SRC_DIR) clean
