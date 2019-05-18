CXX = g++ -std=c++14 -Wall -Wextra 
CXXFLAGS = -O3

TAGDDIR =../../../tagd
INC = -I../include -I../../include -I$(TAGDDIR)/include
SRCS = sqlite.cc
HDRS = ../include/tagdb/sqlite.h
OBJS=$(SRCS:.cc=.o)
BIN = tagdb

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o $(BIN)

