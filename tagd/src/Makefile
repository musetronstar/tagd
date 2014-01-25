CC = gcc
CXX = g++ -std=c++0x
WFLAGS = -Wall -Wextra -g

INC =  -I../include
SRCS = tagd.cc rank.cc url.cc utf8.cc
CODES_H = ../include/tagd/codes.h
HARD_TAGS_H = ../include/tagd/hard-tags.h
HDRS = $(wildcard $(INC).h)
OBJS=$(SRCS:.cc=.o)
HARD_TAG_GPERF=hard-tags.gperf
HARD_TAG_GPERF_H=hard-tags.gperf.h

all : $(HARD_TAG_GPERF_H) codes.inc hard-tag.o $(SRCS) $(OBJS)

codes.inc: $(CODES_H)
	./gen-codes-inc.pl $? > $@

$(HARD_TAG_GPERF_H): $(HARD_TAG_GPERF)
	gperf $? > $@

$(HARD_TAG_GPERF):
	./gen-hard-tags.gperf.pl $(HARD_TAGS_H) > $@

hard-tag.o: hard-tag.cc
	$(CXX) -Wno-missing-field-initializers -c -o $@ $? $(INC) $(WFLAGS)

.cc.o :
	$(CXX) -c -o $@ $< $(INC) $(WFLAGS)

clean :
	rm -f *.o *.inc $(HARD_TAG_GPERF) $(HARD_TAG_GPERF_H)

