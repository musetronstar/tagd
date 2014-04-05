CXX = g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3
YACC = 

INC =  -I../include -I../../tagd/include -I../../tagspace/include
PARSER_H = ../include/parser.h

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

all: build

build: tagl.o parser.o scanner.o

scanner.o: lemonfiles
	re2c scanner.re | $(CXX) -c -o $@  -x c++ - $(INC)

parser.cc: lemonfiles
 
$(PARSER_H): lemonfiles
 
.PHONY: lemonfiles
lemonfiles: parser.y
	lemon parser.y -s
	mv parser.c parser.cc  # or make will launch bison as a prereq
	mv parser.h $(PARSER_H)

# token to string case statements
tokens.inc: $(PARSER_H)
	grep '#define' $? | awk '{printf "case %s: return \"%s\";\n", $$2, $$2;}' > $@

parser.o: $(PARSER_H) parser.cc
	$(CXX) $(CXXFLAGS) -c -o $@ parser.cc $(INC) 

tagl.o: parser.o tokens.inc tagl.cc
	$(CXX) $(CXXFLAGS) -c -o $@ tagl.cc parser.o $(INC) 
 
.PHONY: clean
clean:
	rm -f *.o
	rm -f *.inc
	rm -f parser.cc parser.out $(PARSER_H)

