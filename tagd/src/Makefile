CC = gcc
CXX = g++ -std=c++0x
WFLAGS = -Wall -Wextra -g

INC =  -I../include
SRCS = tagd.cc rank.cc url.cc utf8.cc
CODES_H = ../include/tagd/codes.h
HDRS = $(wildcard $(INC).h)
OBJS=$(SRCS:.cc=.o)

all : codes.inc $(SRCS) $(OBJS)

codes.inc: $(CODES_H)
	./gen-codes-inc.pl $? > $@

.cc.o :
	$(CXX) -c -o $@ $< $(INC) $(WFLAGS)

clean :
	rm -f *.o *.inc 

