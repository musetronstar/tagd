CXX = g++ -g -std=c++0x
WFLAGS = -Wall
DFLAGS = -DNDEBUG
CFLAGS = -fno-strict-aliasing

TAGD_DIR = ../../tagd
TAGL_DIR = ../../tagl
TAGSPACE_DIR = ../../tagdb

TAGSPACE_DIR = ../../tagspace
BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/httagd

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -lsqlite3

LIBEVHTP_DIR = /home/icolley/src/libevhtp
LIBEVHTP_INC = -I$(LIBEVHTP_DIR)/build -I$(LIBEVHTP_DIR) -I$(LIBEVHTP_DIR)/build/compat -I$(LIBEVHTP_DIR)/htparse -I$(LIBEVHTP_DIR)/build/oniguruma -I$(LIBEVHTP_DIR)/oniguruma -I$(LIBEVHTP_DIR)/evthr
LIBEVHTP_LFLAGS = -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lpthread -ldl -lrt
LIBEVHTP = $(LIBEVHTP_DIR)/build/libevhtp.a

all : $(BIN)

# TODO remove -fpermissive to solve error:
# invalid conversion from ‘evhtp_res (*)(evhtp_request_t*, evhtp_path_t*, void*) {aka short unsigned int (*)(evhtp_request_s*, evhtp_path_s*, void*)}’ to ‘evhtp_hook {aka short unsigned int (*)()}’ [-fpermissive]
main.o : main.cc
	$(CXX) -fpermissive -c -o $@ $? $(LIBEVHTP_INC) $(INC) $(CXXFLAGS)
 
$(BIN) : main.o
	$(CXX) -o $@ $? $(CXXFLAGS) $(LIBEVHTP) $(LIBEVHTP_LFLAGS) $(LFLAGS)

clean :
	rm -f *.o $(BIN)
