TAGD_DIR = ../../tagd
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
PRG_DIR = ../sqlite
INC = -I../include -I$(TAGD_DIR)/include -I$(PRG_DIR)/include

CXX = g++ -std=c++14 -Wall -Wextra
CXXTEST = cxxtestgen --error-printer
CXXFLAGS = -O3
LFLAGS = -L../lib -ltagdb-sqlite -L$(TAGD_DIR)/lib -ltagd -lsqlite3 

SRC = tester.cc
BIN = ./tester

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build: $(LIBTAGD)
	$(CXXTEST) Tester.h -o $(SRC)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC) $(INC) $(LFLAGS)
	rm -f *.sqlite
	$(BIN)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f $(SRC) $(BIN) *.sqlite
