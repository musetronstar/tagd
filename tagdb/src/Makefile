# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -O3

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = tagdb.cc
HDRS = ../include/tagdb.h
OBJS=$(SRCS:.cc=.o)

HARD_TAGS_H = ../../tagd/include/tagd/hard-tags.h
HARD_TAG_GPERF=hard-tags.gperf
HARD_TAG_GPERF_H=hard-tags.gperf.h

all: build

debug: CXXFLAGS += -g -DDEBUG -O0
debug: build

build: $(HDRS) $(SRCS) $(OBJS) $(HARD_TAG_GPERF_H) hard-tag.o

.cc.o :
	g++ $(CXXFLAGS) -c -o $@ $< $(INC)

hard-tag.o: hard-tag.cc
	g++ $(CXXFLAGS) -Wno-missing-field-initializers -c -o $@ $? $(INC)

$(HARD_TAG_GPERF_H): $(HARD_TAG_GPERF)
	gperf $? > $@

$(HARD_TAG_GPERF):
	./gen-hard-tags.gperf.pl $(HARD_TAGS_H) > $@

clean:
	rm -f *.o $(HARD_TAG_GPERF) $(HARD_TAG_GPERF_H)

