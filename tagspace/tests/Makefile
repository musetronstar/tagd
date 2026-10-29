include ../backend.mk

INCLUDE_TAGSPACE = ../include
INCLUDE_TAGD = ../../tagd/include

CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
INC = -I$(INCLUDE_TAGSPACE) -I$(INCLUDE_TAGD)
LIBTAGD = ../../tagd/lib/libtagd.a
LIBTAGSPACE = ../lib/libtagspace.a
LFLAGS = -L../lib -ltagspace -L../../tagd/lib -ltagd $(TAGSPACE_BACKEND_LIBS)
CXXTEST = cxxtestgen --error-printer

all: effective_relations_tests type_relations_tests tagspace_memory_tests storage_tests config_tests collection_tests

debug: CXXFLAGS += -g -O0
debug: all

TAGSPACE_MEMORY_TESTER = ./tagspace-memory-tester

tagspace_memory_tests: $(LIBTAGSPACE) $(LIBTAGD) TagspaceMemoryTester.h
	$(CXXTEST) TagspaceMemoryTester.h -o tagspace-memory-tester.cpp
	g++ $(CXXFLAGS) tagspace-memory-tester.cpp -o tagspace-memory-tester $(INC) $(LFLAGS)
	$(TAGSPACE_MEMORY_TESTER)

clean:
	rm -f effective-relations-tester.cpp effective-relations-tester type-relations-tester.cpp type-relations-tester collection-tester.cpp collection-tester tagspace-memory-tester.cpp tagspace-memory-tester storage-tester.cpp storage-tester config-tester.cpp config-tester

storage_tests: $(LIBTAGSPACE) $(LIBTAGD) StorageTester.h
	$(CXXTEST) StorageTester.h -o storage-tester.cpp
	g++ $(CXXFLAGS) storage-tester.cpp -o storage-tester $(INC) $(LFLAGS)
	./storage-tester

config_tests: $(LIBTAGSPACE) $(LIBTAGD) ConfigTester.h
	$(CXXTEST) ConfigTester.h -o config-tester.cpp
	g++ $(CXXFLAGS) config-tester.cpp -o config-tester $(INC) $(LFLAGS)
	./config-tester

collection_tests: $(LIBTAGSPACE) $(LIBTAGD) CollectionTester.h
	$(CXXTEST) CollectionTester.h -o collection-tester.cpp
	g++ $(CXXFLAGS) collection-tester.cpp -o collection-tester $(INC) $(LFLAGS)
	./collection-tester

# Public backend-neutral location types accompany the tagspace API.
INC += $(TAGSPACE_CPPFLAGS)

type_relations_tests: $(LIBTAGSPACE) $(LIBTAGD) TypeRelationsTester.h
	$(CXXTEST) TypeRelationsTester.h -o type-relations-tester.cpp
	g++ $(CXXFLAGS) type-relations-tester.cpp -o type-relations-tester $(INC) $(LFLAGS)
	./type-relations-tester

effective_relations_tests: $(LIBTAGSPACE) $(LIBTAGD) EffectiveRelationsTester.h
	$(CXXTEST) EffectiveRelationsTester.h -o effective-relations-tester.cpp
	g++ $(CXXFLAGS) effective-relations-tester.cpp -o effective-relations-tester $(INC) $(LFLAGS)
	./effective-relations-tester
