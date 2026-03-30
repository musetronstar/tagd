# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

TAGDDIR =../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = tagdb.cc
OBJS=$(SRCS:.cc=.o)
DEPS=$(OBJS:.o=.d)

HARD_TAGS_H = ../../tagd/include/tagd/hard-tags.h
HARD_TAG_GPERF=hard-tags.gperf
HARD_TAG_GPERF_H=hard-tags.gperf.h

all: build

debug: CXXFLAGS += -g -DDEBUG -O0
debug: build

build: $(OBJS) $(HARD_TAG_GPERF_H) hard-tag.o

.cc.o :
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

hard-tag.o: hard-tag.cc $(HARD_TAG_GPERF_H)
	g++ $(CXXFLAGS) -MMD -MP -Wno-missing-field-initializers -c -o $@ $< $(INC)

$(HARD_TAG_GPERF_H): $(HARD_TAG_GPERF)
	gperf $? > $@

$(HARD_TAG_GPERF):
	./gen-hard-tags.gperf.pl $(HARD_TAGS_H) > $@

-include $(DEPS)
-include hard-tag.d

clean:
	rm -f *.o *.d $(HARD_TAG_GPERF) $(HARD_TAG_GPERF_H)
