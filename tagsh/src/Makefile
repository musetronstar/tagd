CXX = g++ -std=c++0x -O3
#DFLAGS =
# -Wno-write-strings suppress const char * to char * warnings
CXXFLAGS = -Wall -Wextra -pedantic -Wno-write-strings $(DFLAGS)
BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/tagsh

TAGSH_DIR = ../
TAGD_DIR = ../../tagd
TAGL_DIR = ../../tagl
TAGSPACE_DIR = ../../tagspace

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include -I$(TAGSH_DIR)/include
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -lsqlite3 -levent

DEBUG=
debug: DEBUG=debug
debug: build

all: build

build : $(BIN)

tagsh.o : tagsh.cc
	$(CXX) -c -o $@ $? $(INC) $(CXXFLAGS)

main.o : main.cc
	$(CXX) -c -o $@ $? $(INC) $(CXXFLAGS)
 
$(BIN) : tagsh.o main.o
	$(CXX) -o $@ $? $(INC) $(CXXFLAGS) $(LFLAGS)

clean :
	rm -f *.o $(BIN)
