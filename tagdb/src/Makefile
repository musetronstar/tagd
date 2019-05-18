CXX = g++ -std=c++14 -Wall -Wextra
CXXFLAGS = -O3 

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = tagdb.cc
HDRS = ../include/tagdb.h
OBJS=$(SRCS:.cc=.o)

HARD_TAGS_H = ../../tagd/include/tagd/hard-tags.h
HARD_TAG_GPERF=hard-tags.gperf
HARD_TAG_GPERF_H=hard-tags.gperf.h

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g -DDEBUG
debug: build

build: $(HDRS) $(SRCS) $(OBJS) $(HARD_TAG_GPERF_H) hard-tag.o

.cc.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

hard-tag.o: hard-tag.cc
	$(CXX) $(CXXFLAGS) -Wno-missing-field-initializers -c -o $@ $? $(INC)

$(HARD_TAG_GPERF_H): $(HARD_TAG_GPERF)
	gperf $? > $@

$(HARD_TAG_GPERF):
	./gen-hard-tags.gperf.pl $(HARD_TAGS_H) > $@

clean:
	rm -f *.o $(HARD_TAG_GPERF) $(HARD_TAG_GPERF_H)
