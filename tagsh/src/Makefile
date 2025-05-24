# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -pedantic -O3
BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/tagsh

TAGD_DIR = ../../tagd
TAGL_DIR = ../../tagl
TAGSPACE_DIR = ../../tagdb

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include -I ../
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagdb-sqlite -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -lsqlite3 -levent -lreadline

all: build

debug: CXXFLAGS += -g -O0
debug: build

build : $(BIN)

tagsh.o : tagsh.cc
	g++ $(CXXFLAGS) -Wno-write-strings -c -o $@ $? $(INC)

main.o : main.cc
	g++ $(CXXFLAGS) -c -o $@ $? $(INC)
 
$(BIN) : tagsh.o main.o
	g++ $(CXXFLAGS) -o $@ $? $(INC) $(LFLAGS)

clean :
	rm -f *.o $(BIN)

