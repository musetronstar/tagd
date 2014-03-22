CXX = g++ -std=c++0x -O3
CXXFLAGS = -Wall -Wextra

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = tagspace.cc bootstrap.cc
HDRS = ../include/tagspace.h ../include/tagspace/bootstrap.h
OBJS=$(SRCS:.cc=.o)

DEBUG=
debug: DEBUG=debug
#debug: CXXFLAGS += -g -DDEBUG
debug: build

all: build

build: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) -c -o $@ $< $(INC) $(CXXFLAGS)

clean:
	rm -f *.o

