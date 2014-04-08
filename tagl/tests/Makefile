TAGD_DIR = ../../tagd
TAGSPACE_DIR = ../../tagspace
INC = -I $(HOME)/src/cxxtest -I../include -I$(TAGD_DIR)/include -I../../tagspace/include 
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a

CXXTEST = $(HOME)/src/cxxtest/bin/cxxtestgen --error-printer
CXX = g++ -std=c++0x 
CXXFLAGS = -O3 -Wno-write-strings
LFLAGS =  -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L../lib -ltagl -L$(TAGD_DIR)/lib -ltagd -levent

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g -Wno-write-strings
debug: build

build: tagl_tests

tagl_tests: $(LIBTAGD)
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) $(CXXFLAGS) -o tester tester.cpp $(INC) $(LFLAGS)
	./tester

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f tester.cpp tester.o ./tester
