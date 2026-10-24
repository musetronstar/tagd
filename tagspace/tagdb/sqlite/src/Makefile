# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

# Project libraries report errors explicitly; tests retain their own compiler flags.
override CXXFLAGS += -fno-exceptions

TAGDDIR =../../../../tagd
INC = -I../include -I../../include -I$(TAGDDIR)/include
SRCS = batch.cc sqlite.cc read-snapshot.cc storage.cc
OBJS=$(SRCS:.cc=.o)
DEPS=$(OBJS:.o=.d)
BIN = tagdb

all: build

debug: CXXFLAGS += -g -O0
debug: build

build: $(OBJS)

.cc.o :
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

-include $(DEPS)

clean:
	rm -f *.o *.d $(BIN)
