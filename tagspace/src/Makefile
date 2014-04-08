CXX = g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3 

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = tagspace.cc bootstrap.cc
HDRS = ../include/tagspace.h ../include/tagspace/bootstrap.h
OBJS=$(SRCS:.cc=.o)

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g -DDEBUG
debug: build

build: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o

