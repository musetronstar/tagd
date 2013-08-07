CC = gcc
CXX = g++
WFLAGS = -Wall -Wextra -g
#OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))

INC =  -I../include
SRCS = tagd.cc rank.cc url.cc
HDRS = $(wildcard $(INC).h)
OBJS=$(SRCS:.cc=.o)
#BIN = tagd

all : $(SRCS) $(OBJS)

.cc.o :
	$(CXX) -c -o $@ $< $(INC) $(WFLAGS)

#$(BIN) : $(OBJS)
#	$(CXX) -o $@ $? $(INC)

clean :
	rm -f *.o 

