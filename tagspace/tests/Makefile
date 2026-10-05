INCLUDE_TAGSPACE = ../include
INCLUDE_TAGD = ../../tagd/include

CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
INC = -I$(INCLUDE_TAGSPACE) -I$(INCLUDE_TAGD)
LIBTAGD = ../../tagd/lib/libtagd.a
LIBTAGSPACE = ../lib/libtagspace.a
LFLAGS = -L../lib -ltagspace -L../../tagd/lib -ltagd -lsqlite3
CXXTEST = cxxtestgen --error-printer

all: tagspace_memory_tests storage_tests

debug: CXXFLAGS += -g -O0
debug: all

TAGSPACE_MEMORY_TESTER = ./tagspace-memory-tester

tagspace_memory_tests: $(LIBTAGSPACE) $(LIBTAGD) TagspaceMemoryTester.h
	$(CXXTEST) TagspaceMemoryTester.h -o tagspace-memory-tester.cpp
	g++ $(CXXFLAGS) tagspace-memory-tester.cpp -o tagspace-memory-tester $(INC) $(LFLAGS)
	$(TAGSPACE_MEMORY_TESTER)

clean:
	rm -f tagspace-memory-tester.cpp tagspace-memory-tester storage-tester.cpp storage-tester

storage_tests: $(LIBTAGSPACE) $(LIBTAGD) StorageTester.h
	$(CXXTEST) StorageTester.h -o storage-tester.cpp
	g++ $(CXXFLAGS) storage-tester.cpp -o storage-tester $(INC) $(LFLAGS)
	./storage-tester
