# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -O3
YACC = 
RE2CFLAGS = 

INC =  -I../include -I../../tagd/include -I../../tagdb/include
PARSER_H = ../include/parser.h

all: build

debug: CXXFLAGS += -g -O0
debug: RE2CFLAGS = -d
debug: build

build: tagl.o parser.o scanner.o

scanner.out.cc: scanner.re.cc
	re2c $(RE2CFLAGS) -o scanner.out.cc scanner.re.cc

scanner.o: scanner.out.cc lemonfiles
	g++ $(CXXFLAGS) -c -o $@ scanner.out.cc $(INC) 

parser.cc: lemonfiles
 
$(PARSER_H): lemonfiles
 
.PHONY: lemonfiles
lemonfiles: lemon lempar.cc parser.y
	./lemon parser.y -s -Tlempar.cc
	mv parser.c parser.cc  # or make will launch bison as a prereq
	perl -pi -e 's/parser\.c/parser\.cc/g' parser.cc # replace the #line statements (for debugging)
	mv parser.h $(PARSER_H)

lemon: lemon.c
	gcc $? -o $@

# token to string case statements
tokens.inc: $(PARSER_H)
	grep '#define' $? | awk '{printf "case %s: return \"%s\";\n", $$2, $$2;}' > $@

parser.o: $(PARSER_H) parser.cc
	# we have to silence certain lemon warnings/errors
	# -Wno-sign-compare : int / unsigned long comparisons
	# -fpermissive : *yyParser to unsigned char conversion
	g++ $(CXXFLAGS) -Wno-sign-compare -fpermissive -c -o $@ parser.cc $(INC) 

tagl.o: tokens.inc tagl.cc
	g++ $(CXXFLAGS) -c -o $@ tagl.cc $(INC) 
 
.PHONY: clean
clean:
	rm -f *.o
	rm -f *.inc
	rm -f scanner.out.cc parser.cc parser.out $(PARSER_H)

