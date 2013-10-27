CXX = g++ -g -std=c++0x
WFLAGS = -Wall -Wextra
YACC = 

INC =  -I../include -I../../tagd/include -I../../tagspace/include
CXXFLAGS += $(INC)

all: tagl.o parser.o scanner.o

scanner.o: lemonfiles
	re2c scanner.re | $(CXX) -c -o $@  -x c++ - $(CXXFLAGS)

tagl.o: tagl.cc parser.cc parser.h
	$(CXX) -c -o $@ tagl.cc $(CXXFLAGS) $(WFLAGS)
 
parser.cc: lemonfiles
 
parser.h: lemonfiles
 
.PHONY: lemonfiles
lemonfiles: parser.y
	lemon parser.y -s
	mv parser.c parser.cc  # or make will launch bison as a prereq
	mv parser.h ../include
 
.PHONY: clean
clean:
	rm -f *.o
	rm -f parser.cc parser.out ../include/parser.h 

