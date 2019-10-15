CXX = g++ -std=c++14 -Wall -Wextra
CXXFLAGS = -O3

PRG_DIR = ../..
TAGD_DIR = $(PRG_DIR)/tagd
TAGL_DIR = $(PRG_DIR)/tagl
TAGSPACE_DIR = $(PRG_DIR)/tagdb
TAGSH_DIR = $(PRG_DIR)/tagsh
HTTAGD_DIR = $(PRG_DIR)/httagd

BIN_DIR = ../bin
BIN = $(BIN_DIR)/httagd

INC = -I${HTTAGD_DIR}/include \
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

ifdef EVHTP_SRC_DIR
	INC += -I$(EVHTP_SRC_DIR)/include -I$(EVHTP_SRC_DIR)/build/include
	LFLAGS += -L$(EVHTP_SRC_DIR)/build
else
	INC += -I/usr/include/evhtp
endif

LFLAGS += -levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lpthread -ldl -lrt

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build : $(BIN)

main.o : main.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $? $(INC)

$(BIN) : main.o
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $? $(LFLAGS)

clean :
	rm -f *.o
	rm -f $(BIN)
