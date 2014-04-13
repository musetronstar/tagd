CXX = g++ -std=c++0x -Wall -Wextra -pedantic
# -Wno-write-strings suppress const char * to char * warnings
CXXFLAGS = -O3
BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/tagsh

TAGD_DIR = ../../tagd
TAGL_DIR = ../../tagl
TAGSPACE_DIR = ../../tagspace

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include -I ../
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -lsqlite3 -levent -lreadline

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build : $(BIN)

tagsh.o : tagsh.cc
	$(CXX) $(CXXFLAGS) -Wno-write-strings -c -o $@ $? $(INC)

main.o : main.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $? $(INC)
 
$(BIN) : tagsh.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $? $(INC) $(LFLAGS)

clean :
	rm -f *.o $(BIN)
