# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
YACC =
RE2CFLAGS =

INC =  -I../include -I../../tagd/include -I../../tagspace/include
PARSER_H = ../include/parser.h

OBJS = tagl.o parser.o scanner_runtime.o scanner_lexer.o tagdurl_lexer.o
DEPS = $(OBJS:.o=.d)

all: build

debug: CXXFLAGS += -g -O0
debug: RE2CFLAGS = -d
debug: build

build: tagl.o parser.o scanner_runtime.o scanner_lexer.o tagdurl_lexer.o

scanner.out.cc: scanner.re.cc ../include/scanner.h
	re2c $(RE2CFLAGS) -I../include -o scanner.out.cc scanner.re.cc

scanner.tagdurl.cc: tagdurl.re.cc ../include/scanner.h
	re2c $(RE2CFLAGS) -I../include -o scanner.tagdurl.cc tagdurl.re.cc

scanner_runtime.o: scanner.cc
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

scanner_lexer.o: scanner.out.cc $(PARSER_H)
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ scanner.out.cc $(INC)

tagdurl_lexer.o: scanner.tagdurl.cc $(PARSER_H)
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ scanner.tagdurl.cc $(INC)

# lemon generates parser.c and parser.h in one shot; list parser.c here too so
# make does not fall back to its built-in yacc rule for parser.y -> parser.c
parser.c parser.cc $(PARSER_H): lemon lempar.cc parser.y
	./lemon parser.y -s -Tlempar.cc
	mv parser.c parser.cc  # or make will launch bison as a prereq
	perl -pi -e 's/parser\.c/parser\.cc/g' parser.cc # replace the #line statements (for debugging)
	mv parser.h $(PARSER_H)

lemon: lemon.c
	gcc $< -o $@

# token to string case statements
tokens.inc: $(PARSER_H)
	grep '#define' $? | awk '{printf "case %s: return \"%s\";\n", $$2, $$2;}' > $@

parser.o: $(PARSER_H) parser.cc
	# we have to silence certain lemon warnings/errors
	# -Wno-sign-compare : int / unsigned long comparisons
	# -Wno-unused-parameter : generated yy_destructor() keeps yypminor in the
	#   signature even when current Lemon destructors are NOOP; keep this scoped
	#   to parser.cc so handwritten code still reports unused parameters
	# -fpermissive : *yyParser to unsigned char conversion
	g++ $(CXXFLAGS) -MMD -MP -Wno-sign-compare -Wno-array-bounds -Wno-unused-variable -Wno-unused-parameter -fpermissive -c -o $@ parser.cc $(INC)

tagl.o: tokens.inc tagl.cc
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ tagl.cc $(INC)

-include $(DEPS)

.PHONY: clean
clean:
	rm -f *.o *.d
	rm -f *.inc
	rm -f lemon scanner.out.cc scanner.tagdurl.cc parser.c parser.cc parser.out $(PARSER_H)
