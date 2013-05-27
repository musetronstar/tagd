CXX = g++
CFLAGS = -Wall -Wextra -g

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = bootstrap.cc
HDRS = ../include/tagspace.h ../include/tagspace/bootstrap.h
OBJS=$(SRCS:.cc=.o)

all: $(HDRS) $(SRCS) $(OBJS)

.cc.o :
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f *.o

