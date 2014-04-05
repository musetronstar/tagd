CXX = g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3
CXXTEST = $(HOME)/src/cxxtest/bin/cxxtestgen --error-printer
INC = -I $(HOME)/src/cxxtest -I../ -I../../../include
OBJS = ../*.o

TESTER_CPP = tester.cpp
TESTER_BIN = ./tester

all: build

debug: CXXFLAGS = -g
debug: build

build:
	$(CXXTEST) Tester.h -o $(TESTER_CPP)
	$(CXX) $(CXXFLAGS) -o $(TESTER_BIN) $(OBJS) $(TESTER_CPP) $(INC)
	$(TESTER_BIN)

clean:
	rm -f $(TESTER_CPP) $(TESTER_BIN)
