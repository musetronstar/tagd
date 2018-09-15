SRC_DIR = ./src
APP_DIR = ./app
BIN_DIR = ./bin
LIB_DIR = ./lib
TEST_DIR = ./tests

LIB = $(LIB_DIR)/libhttagd.a

all: build

DEBUG=
debug: DEBUG=debug
debug: build

# if defined, will propogate to subsequent make targets
# otherwise,EVHTP should be installed globally, ie: /usr/include/evhtp
#EVHTP_ARGS=EVHTP_SRC_DIR=$(HOME)/src/libevhtp

build: src lib

src: force_look
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	make -C	$(SRC_DIR) $(DEBUG) $(EVHTP_ARGS)

app: force_look lib
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	make -C	$(APP_DIR) $(DEBUG) $(EVHTP_ARGS)

tests: force_look app
	make -C $(TEST_DIR) $(DEBUG) $(EVHTP_ARGS)

lib: src $(LIB)

$(LIB):
	[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	ar rcs $(LIB) $(SRC_DIR)/*.o

force_look:
	true

clean:
	rm -rf  $(LIB_DIR) $(BIN_DIR)
	make -C $(SRC_DIR) clean
	make -C $(APP_DIR) clean
