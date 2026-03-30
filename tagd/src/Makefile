# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

INC =  -I../include
SRCS = tagd.cc rank.cc utf8.cc url.cc io.cc file.cc
CODES_H = ../include/tagd/codes.h
OBJS=$(SRCS:.cc=.o)
DEPS=$(OBJS:.o=.d)

all : build

debug : CXXFLAGS += -g -O0
debug : build

build : $(HARD_TAG_GPERF_H) tagd-codes.inc part-of-speech.inc $(OBJS)

tagd-codes.inc: $(CODES_H)
	./gen-codes-inc.pl TAGD_CODES $? > $@

part-of-speech.inc: $(CODES_H)
	./gen-codes-inc.pl PART_OF_SPEECH $? > $@

.cc.o :
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

-include $(DEPS)

clean :
	rm -f *.o *.d *.inc
