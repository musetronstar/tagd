CXX = g++ -g
WFLAGS = -Wall -Wextra

INC =  -I../include -I../../tagd/include -I../../tagspace/include
CXXFLAGS += $(INC)

all: tagl.o taglparser.o scanner.o

scanner.o: lemonfiles
	re2c scanner.re | g++ -c -o $@  -x c++ - $(CXXFLAGS)

tagl.o: tagl.cc taglparser.cc taglparser.h
	$(CXX) -c -o $@ tagl.cc $(CXXFLAGS) $(WFLAGS)
 
taglparser.cc: lemonfiles
 
taglparser.h: lemonfiles
 
.PHONY: lemonfiles
lemonfiles: taglparser.y
	lemon taglparser.y -s
	mv taglparser.c taglparser.cc  # or make will launch bison as a prereq
	mv taglparser.h ../include
 
.PHONY: clean
clean:
	rm -f *.o
	rm -f taglparser.cc taglparser.out ../include/taglparser.h 

