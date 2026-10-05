# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

TAGDDIR =../../../tagd
INC = -I../include -I$(TAGDDIR)/include
SRCS = tagdb.cc
OBJS=$(SRCS:.cc=.o)
DEPS=$(OBJS:.o=.d)

HARD_TAGS_H = ../../../tagd/include/tagd/hard-tags.h

all: build

debug: CXXFLAGS += -g -DDEBUG -O0
debug: build

build: $(OBJS) hard-tag.o

.cc.o :
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

hard-tag.o: hard-tag.cc $(HARD_TAGS_H)
	g++ $(CXXFLAGS) -MMD -MP -Wno-missing-field-initializers -c -o $@ $< $(INC)

-include $(DEPS)
-include hard-tag.d

clean:
	rm -f *.o *.d
