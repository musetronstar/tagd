CXX = g++ -std=c++0x
CXXTEST = $(HOME)/cxxtest/bin/cxxtestgen --error-printer
CFLAGS = -DDEBUG -ggdb
INC = -I $(HOME)/cxxtest -I../ -I../../../include
OBJS = ../*.o

TESTER_CPP = tester.cpp
TESTER_BIN = ./tester

all:
	$(CXXTEST) Tester.h -o $(TESTER_CPP)
	$(CXX) $(CFLAGS) -o $(TESTER_BIN) $(OBJS) $(TESTER_CPP) $(INC)
	$(TESTER_BIN)

clean:
	rm -f $(TESTER_CPP) $(TESTER_BIN)
