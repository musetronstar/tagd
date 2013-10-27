CXX = g++ -g -std=c++0x
#DFLAGS =
FLAGS = -DDEBUG
# -Wno-write-strings suppress const char * to char * warnings
CXXFLAGS = -Wall -Wextra -pedantic -Wno-write-strings $(DFLAGS)
BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/tagsh

TAGD_DIR = ../../tagd
TAGL_DIR = ../../tagl
TAGSPACE_DIR = ../../tagspace

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -lsqlite3 -levent

all : $(BIN)

main.o : main.cc
	$(CXX) -c -o $@ $? $(INC) $(CXXFLAGS)
 
$(BIN) : main.o
	$(CXX) -o $@ $? $(INC) $(CXXFLAGS) $(LFLAGS)

clean :
	rm -f *.o $(BIN)
