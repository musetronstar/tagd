TAGD_DIR = ../../tagd
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
PRG_DIR = ../sqlite
INC = -I../include -I$(TAGD_DIR)/include -I$(PRG_DIR)/include -I $(HOME)/src/cxxtest

CXX = g++ -std=c++0x -O3
CXXTEST = $(HOME)/src/cxxtest/bin/cxxtestgen --error-printer
CXXFLAGS = 
LFLAGS = -L../lib -ltagspace-sqlite -L$(TAGD_DIR)/lib -ltagd -lsqlite3 

SRC = tester.cc
OBJ = tester.o
BIN = ./tester

DEBUG=
debug: DEBUG=debug
#debug: CXXFLAGS += -DDEBUG -ggdb
debug: build

all: build

build: $(LIBTAGD)
	$(CXXTEST) Tester.h -o $(SRC)
	$(CXX) -c -o $(OBJ) $(SRC) $(CXXFLAGS) $(INC)
	$(CXX) $(OBJ) -o $(BIN) $(CXXFLAGS) $(LFLAGS)
	rm -f *.sqlite
	$(BIN)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f $(SRC) $(OBJ) $(BIN) *.sqlite
