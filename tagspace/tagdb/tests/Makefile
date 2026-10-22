include ../../backend.mk

TAGD_DIR = ../../../tagd
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
LIBTAGDB = ../lib/lib$(TAGDB_LIBRARY).a
PRG_DIR = ../$(TAGDB_BACKEND)
INC = -I../include -I$(TAGD_DIR)/include -I$(PRG_DIR)/include

# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
CXXTEST = cxxtestgen --error-printer
LFLAGS = -L../lib -l$(TAGDB_LIBRARY) -L$(TAGD_DIR)/lib -ltagd $(TAGSPACE_BACKEND_LIBS)

# SQL diagnostics and fault injection belong only to the SQLite implementation.
ifeq ($(TAGDB_BACKEND),sqlite)
BACKEND_TESTS = SqliteTester.h SqliteCacheTester.h
endif

SRC = tester.cc
BIN = ./tester
TESTER = $(BIN)

all: build

debug: CXXFLAGS += -g -O0
debug: all

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) $(BIN)
valgrind: debug

build: $(LIBTAGD) $(LIBTAGDB)
	$(CXXTEST) Tester.h $(BACKEND_TESTS) CanonicalCacheTester.h CacheContractTester.h AlternativeBackendTester.h -o $(SRC)
	g++ $(CXXFLAGS) -o $(BIN) $(SRC) $(INC) $(LFLAGS)
	$(TESTER)
	$(CXXTEST) AlternativeBackendTester.h CacheContractTester.h -o alternative-tester.cc
	g++ $(CXXFLAGS) -o alternative-tester alternative-tester.cc -I../include -I$(TAGD_DIR)/include ../src/canonical-cache.o -L$(TAGD_DIR)/lib -ltagd
	./alternative-tester

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f $(SRC) $(BIN) alternative-tester.cc alternative-tester *.sqlite
