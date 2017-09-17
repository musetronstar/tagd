TAGD_DIR = ../../tagd
TAGSPACE_DIR = ../../tagdb
TAGL_DIR = ../../tagl
TAGSH_DIR = ../../tagsh
HTTAGD_DIR = ../../httagd

INC  = -I $(HOME)/src/cxxtest
INC += -I../include -I$(TAGD_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include -I$(TAGL_DIR)/include -I$(TAGSH_DIR)/include
LIBHTTAGD = $(HTTAGD_DIR)/lib/libhttagd.a

CXXTEST = $(HOME)/src/cxxtest/bin/cxxtestgen --error-printer
CXX = g++ -std=c++0x 
CXXFLAGS = -O3 -Wno-write-strings
LFLAGS  =
LFLAGS += -L$(HTTAGD_DIR)/lib -lhttagd
LFLAGS += -L$(TAGSPACE_DIR)/lib -ltagdb-sqlite
LFLAGS += -L$(TAGD_DIR)/lib -ltagd
LFLAGS += -L$(TAGL_DIR)/lib -ltagl
LFLAGS += -L$(TAGSH_DIR)/lib -ltagsh

LFLAGS +=  -lsqlite3 -levent -lreadline
LFLAGS += -lctemplate_nothreads

LIBEVHTP_DIR = $(HOME)/src/libevhtp
LIBEVHTP_INC = -I$(LIBEVHTP_DIR)/include -I$(LIBEVHTP_DIR)/build/include
LIBEVHTP_LFLAGS = -L$(LIBEVHTP_DIR)/build -levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lpthread -ldl -lrt

INC += $(LIBEVHTP_INC)
LFLAGS += $(LIBEVHTP_LFLAGS)

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g -Wno-write-strings
debug: build

build: cxx_tests exp_tests

cxx_tests: $(LIBHTTAGD)
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) $(CXXFLAGS) -c -o tester.o tester.cpp $(INC)
	$(CXX) $(CXXFLAGS) -o tester tester.o $(LFLAGS)
	./tester

exp_tests:
	./tester.exp

$(LIBHTTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f tester.cpp tester.o ./tester
