SRC_DIR = ./src

DEBUG=
debug: DEBUG=debug
debug: build

all: build

build: force_look
	make -C	$(SRC_DIR) $(DEBUG)

force_look:
	true

clean:
	make -C $(SRC_DIR) clean

