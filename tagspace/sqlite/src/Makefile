CXX = g++ -std=c++0x -O3
CFLAGS = -Wall -Wextra 

TAGDDIR =../../../tagd
INC = -I../include -I../../include -I$(TAGDDIR)/include
SRCS = sqlite.cc
HDRS = ../include/tagspace/sqlite.h
OBJS=$(SRCS:.cc=.o)
BIN = tagdb

DEBUG=
debug: DEBUG=debug
#debug: -g
debug: build

all: build

build: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o $(BIN)

