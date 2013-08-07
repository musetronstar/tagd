TAGD_DIR = ../../tagd
LIBTAGD = $(TAGD_DIR)/build/libtagd.a
PRG_DIR = ../sqlite
INC = -I../include -I$(TAGD_DIR)/include -I$(PRG_DIR)/include -I ~/src/cxxtest

CXX = g++
CXXTEST=~/src/cxxtest/bin/cxxtestgen --error-printer
CXXFLAGS = -DDEBUG -ggdb
LFLAGS = -L../build -ltagspace-sqlite -L$(TAGD_DIR)/build -ltagd -lsqlite3 

SRC = tester.cc
OBJ = tester.o
BIN = ./tester

all: $(LIBTAGD)
	$(CXXTEST) Tester.h -o $(SRC)
	$(CXX) -c -o $(OBJ) $(SRC) $(CXXFLAGS) $(INC)
	$(CXX) $(OBJ) -o $(BIN) $(CXXFLAGS) $(LFLAGS)
	rm -f *.sqlite
	$(BIN)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f $(SRC) $(OBJ) $(BIN) *.sqlite
