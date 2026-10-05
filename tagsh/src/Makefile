# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -pedantic -O3
BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/tagsh

TAGD_DIR = ../../tagd
TAGL_DIR = ../../tagl
TAGSPACE_DIR = ../../tagspace

LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
LIBTAGL = $(TAGL_DIR)/lib/libtagl.a
LIBTAGSPACE = $(TAGSPACE_DIR)/lib/libtagspace.a

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I ../
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagspace -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -lsqlite3 -levent -lreadline

OBJS = tagsh.o main.o
DEPS = $(OBJS:.o=.d)

all: build

debug: CXXFLAGS += -g -O0
debug: build

build : $(BIN)

tagsh.o : tagsh.cc
	g++ $(CXXFLAGS) -MMD -MP -Wno-write-strings -c -o $@ $< $(INC)

main.o : main.cc
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

$(BIN) : tagsh.o main.o $(LIBTAGD) $(LIBTAGL) $(LIBTAGSPACE)
	g++ $(CXXFLAGS) -o $@ tagsh.o main.o $(INC) -Wl,--start-group $(LFLAGS) -Wl,--end-group

-include $(DEPS)

clean :
	rm -f *.o *.d $(BIN)
