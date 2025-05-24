# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -O3

TAGDDIR =../../../tagd
INC = -I../include -I../../include -I$(TAGDDIR)/include
SRCS = sqlite.cc
HDRS = ../include/tagdb/sqlite.h
OBJS=$(SRCS:.cc=.o)
BIN = tagdb

all: build

debug: CXXFLAGS += -g -O0
debug: build

build: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	g++ $(CXXFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o $(BIN)

