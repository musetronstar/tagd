CXX = g++
CXXFLAGS = -Wall -Wextra -g -DDEBUG

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = bootstrap.cc
HDRS = ../include/tagspace.h ../include/tagspace/bootstrap.h
OBJS=$(SRCS:.cc=.o)

all: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) -c -o $@ $< $(INC) $(CXXFLAGS)

clean:
	rm -f *.o

