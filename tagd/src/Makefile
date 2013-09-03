CC = gcc
CXX = g++
WFLAGS = -Wall -Wextra -g
#OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))

INC =  -I../include
SRCS = tagd.cc rank.cc url.cc
CODES_H = ../include/tagd/codes.h
HDRS = $(wildcard $(INC).h)
OBJS=$(SRCS:.cc=.o)
#BIN = tagd

all : codes.inc $(SRCS) $(OBJS)

codes.inc: $(CODES_H)
	./gen-codes-inc.pl $? > $@

.cc.o :
	$(CXX) -c -o $@ $< $(INC) $(WFLAGS)

#$(BIN) : $(OBJS)
#	$(CXX) -o $@ $? $(INC)

clean :
	rm -f *.o *.inc 

