CXX = g++ -std=c++14 -Wall -Wextra
CXXFLAGS = -O3 

INC =  -I../include
SRCS = tagd.cc rank.cc url.cc utf8.cc
CODES_H = ../include/tagd/codes.h
HDRS = $(wildcard $(INC).h)
OBJS=$(SRCS:.cc=.o)

all : build

debug : CXXFLAGS =  -g
debug : build

build : $(HARD_TAG_GPERF_H) tagd-codes.inc part-of-speech.inc $(SRCS) $(OBJS)

tagd-codes.inc: $(CODES_H)
	./gen-codes-inc.pl TAGD_CODES $? > $@

part-of-speech.inc: $(CODES_H)
	./gen-codes-inc.pl PART_OF_SPEECH $? > $@

.cc.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

clean :
	rm -f *.o *.inc

