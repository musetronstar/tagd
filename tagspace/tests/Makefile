TAGD_DIR = ../../tagd
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
PRG_DIR = ../sqlite
INC = -I../include -I$(TAGD_DIR)/include -I$(PRG_DIR)/include -I $(HOME)/src/cxxtest

CXX = g++ -std=c++0x -Wall -Wextra
CXXTEST = $(HOME)/src/cxxtest/bin/cxxtestgen --error-printer
CXXFLAGS = -O3
LFLAGS = -L../lib -ltagspace-sqlite -L$(TAGD_DIR)/lib -ltagd -lsqlite3 

SRC = tester.cc
OBJ = tester.o
BIN = ./tester

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build: $(LIBTAGD)
	$(CXXTEST) Tester.h -o $(SRC)
	$(CXX) $(CXXFLAGS) -c -o $(OBJ) $(SRC) $(INC)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(BIN) $(LFLAGS)
	rm -f *.sqlite
	$(BIN)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f $(SRC) $(OBJ) $(BIN) *.sqlite
