TAGD_DIR = ../../tagd
TAGSPACE_DIR = ../../tagspace
INC = -I../include -I$(TAGD_DIR)/include -I../../tagspace/include
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a

CXXTEST = $(HOME)/cxxtest/bin/cxxtestgen --error-printer
CXX = g++ -std=c++0x
CXXFLAGS = -DDEBUG -ggdb -Wno-write-strings -I $(HOME)/cxxtest $(INC)
LFLAGS =  -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L../lib -ltagl -L$(TAGD_DIR)/lib -ltagd -levent

all: tagl_tests

tagl_tests: $(LIBTAGD)
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) -o tester tester.cpp $(CXXFLAGS) $(LFLAGS)
	./tester

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f tester.cpp tester.o ./tester
