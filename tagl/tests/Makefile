TAGD_DIR = ../../tagd
INC = -I../include -I$(TAGD_DIR)/include -I../../tagspace/include
LIBTAGD = $(TAGD_DIR)/build/libtagd.a

CXXTEST=~/src/cxxtest/bin/cxxtestgen --error-printer
CXX = g++
CXXFLAGS = -DDEBUG -ggdb -Wno-write-strings -I ~/src/cxxtest $(INC)
LFLAGS =  -L../build -ltagl -L$(TAGD_DIR)/build -ltagd

all: tagl_tests

tagl_tests: $(LIBTAGD)
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) -o tester tester.cpp $(CXXFLAGS) $(LFLAGS)
	./tester

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f tester.cpp tester.o ./tester
