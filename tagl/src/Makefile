CXX = g++ -std=c++0x -O3
WFLAGS = -Wall -Wextra
YACC = 

INC =  -I../include -I../../tagd/include -I../../tagspace/include
CXXFLAGS += $(INC)
PARSER_H = ../include/parser.h

DEBUG=
debug: DEBUG=debug
debug: build

all: build

build: tagl.o parser.o scanner.o

scanner.o: lemonfiles
	re2c scanner.re | $(CXX) -c -o $@  -x c++ - $(CXXFLAGS)

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

tagl.o: tokens.inc tagl.cc parser.cc $(PARSER_H)
	$(CXX) -c -o $@ tagl.cc $(CXXFLAGS) $(WFLAGS)
 
.PHONY: clean
clean:
	rm -f *.o
	rm -f *.inc
	rm -f parser.cc parser.out $(PARSER_H)

