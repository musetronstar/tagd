CXX = g++
CFLAGS = -Wall -Wextra -g

TAGDDIR =../../../tagd
INC = -I../include -I../../include -I$(TAGDDIR)/include
SRCS = sqlite.cc
HDRS = ../include/tagspace/sqlite.h
OBJS=$(SRCS:.cc=.o)
BIN = tagdb

all: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o $(BIN)

