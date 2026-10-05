CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

INC = -I../include -I../../tagd/include -I../tagdb/include -I../tagdb/sqlite/include
SRCS = tagspace.cc
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
