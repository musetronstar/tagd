TAGD_DIR = ../../tagd
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
PRG_DIR = ../sqlite
INC = -I../include -I$(TAGD_DIR)/include -I$(PRG_DIR)/include

# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -O3
CXXTEST = cxxtestgen --error-printer
LFLAGS = -L../lib -ltagdb-sqlite -L$(TAGD_DIR)/lib -ltagd -lsqlite3 

SRC = tester.cc
BIN = ./tester
TESTER = $(BIN)

all: build

debug: CXXFLAGS += -g -O0
debug: all

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) $(BIN)
valgrind: debug

build: $(LIBTAGD)
	$(CXXTEST) Tester.h -o $(SRC)
	g++ $(CXXFLAGS) -o $(BIN) $(SRC) $(INC) $(LFLAGS)
	$(TESTER)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f $(SRC) $(BIN) *.sqlite

