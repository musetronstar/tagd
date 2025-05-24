SRC_DIR = ./src
APP_DIR = ./app
BIN_DIR = ./bin
LIB_DIR = ./lib
TEST_DIR = ./tests

LIB = $(LIB_DIR)/libhttagd.a
# if defined, will propogate to subsequent make targets
# otherwise,EVHTP should be installed globally, ie: /usr/include/evhtp
#EVHTP_ARGS=EVHTP_SRC_DIR=$(HOME)/src/libevhtp

TARGET=

all: build

debug: TARGET = debug
debug: all

valgrind: debug
	make -C $(TEST_DIR) $(EVHTP_ARGS) valgrind

build: src lib app

src: force_look
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	make -C	$(SRC_DIR) $(EVHTP_ARGS) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

app: force_look lib
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	make -C	$(APP_DIR) $(EVHTP_ARGS) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

tests: force_look app
	make -C $(TEST_DIR) $(EVHTP_ARGS) $(TARGET) CXXFLAGS="$(CXXFLAGS)"

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
	make -C $(TEST_DIR) clean

