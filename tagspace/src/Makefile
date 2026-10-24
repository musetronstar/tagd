CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

# Project libraries report errors explicitly; tests retain their own compiler flags.
override CXXFLAGS += -fno-exceptions

INC = -I../include -I../../tagd/include -I../tagdb/include
SRCS = tagspace.cc config.cc collection.cc
OBJS = $(SRCS:.cc=.o)
DEPS = $(OBJS:.o=.d)

all: build

debug: CXXFLAGS += -g -O0
debug: build

build: $(OBJS)

.cc.o:
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

-include $(DEPS)

clean:
	rm -f *.o *.d
