CXX = g++ -std=c++0x -Wall -Wextra 
CXXFLAGS = -O3

TAGDDIR =../../../tagd
INC = -I../include -I../../include -I$(TAGDDIR)/include
SRCS = sqlite.cc
HDRS = ../include/tagspace/sqlite.h
OBJS=$(SRCS:.cc=.o)
BIN = tagdb

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

all: build

build: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o $(BIN)

