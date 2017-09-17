CXX = g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3

PRG_DIR = ../..
TAGD_DIR = $(PRG_DIR)/tagd
TAGL_DIR = $(PRG_DIR)/tagl
TAGSPACE_DIR = $(PRG_DIR)/tagdb
TAGSH_DIR = $(PRG_DIR)/tagsh
HTTAGD_DIR = $(PRG_DIR)/httagd

BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/httagd

INC = -I../include \
	  -I$(TAGD_DIR)/include \
	  -I$(TAGL_DIR)/include \
	  -I$(TAGSPACE_DIR)/include \
	  -I$(TAGSPACE_DIR)/sqlite/include \
	  -I$(TAGSH_DIR)/include

LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagdb-sqlite \
		 -L $(TAGL_DIR)/lib -ltagl \
		 -L $(TAGD_DIR)/lib -ltagd \
		 -L $(TAGSH_DIR)/lib -ltagsh \
		 -L $(HTTAGD_DIR)/lib -lhttagd \
		 -lsqlite3 -levent -lreadline -lctemplate_nothreads

LIBEVHTP_DIR = $(HOME)/src/libevhtp
LIBEVHTP_INC = -I$(LIBEVHTP_DIR)/include -I$(LIBEVHTP_DIR)/build/include
LIBEVHTP_LFLAGS = -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lpthread -ldl -lrt
LIBEVHTP = $(LIBEVHTP_DIR)/build/libevhtp.a

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build : $(BIN)

main.o : main.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $? $(INC) $(LIBEVHTP_INC)

$(BIN) : main.o
	$(CXX) $(CXXFLAGS) -o $@ $? $(LFLAGS) $(LIBEVHTP) $(LIBEVHTP_LFLAGS)

clean :
	rm -f *.o
	rm -f $(BIN)
