TAGD_DIR = ../../tagd
TAGSPACE_DIR = ../../tagdb
INC = -I../include -I$(TAGD_DIR)/include -I../../tagdb/include
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a

CXXTEST = cxxtestgen --error-printer
CXX = g++ -std=c++14
CXXFLAGS = -O3 -Wno-write-strings
LFLAGS =  -L $(TAGSPACE_DIR)/lib -ltagdb-sqlite -L../lib -ltagl -L$(TAGD_DIR)/lib -ltagd -levent

TESTER = ./tester

debug: CXXFLAGS = -g -Wno-write-strings
debug: all

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) ./tester
valgrind: debug

all: build

build: tagl_tests

tagl_tests: $(LIBTAGD)
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) $(CXXFLAGS) -o tester tester.cpp $(INC) $(LFLAGS)
	$(TESTER)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f tester.cpp tester.o ./tester
