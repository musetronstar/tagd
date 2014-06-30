CXX = g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3 

INC =  -I../include
SRCS = tagd.cc rank.cc url.cc utf8.cc
CODES_H = ../include/tagd/codes.h
HDRS = $(wildcard $(INC).h)
OBJS=$(SRCS:.cc=.o)

all : build

debug : CXXFLAGS =  -g
debug : build

build : $(HARD_TAG_GPERF_H) codes.inc $(SRCS) $(OBJS)

codes.inc: $(CODES_H)
	./gen-codes-inc.pl $? > $@

.cc.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

clean :
	rm -f *.o *.inc

