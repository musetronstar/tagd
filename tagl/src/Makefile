CXX = g++ -g
WFLAGS = -Wall -Wextra

INC =  -I../include -I../../tagd/include -I../../tagspace/include
CXXFLAGS += $(INC)

#tagl: tagl.o taglparser.o taglscanner.yy.o main.cc
#	$(CXX)  -o $@ $^ $(CXXFLAGS) $(WFLAGS)

all: tagl.o taglparser.o scanner.o

#tokens.inc: lemonfiles
#	awk 'BEGIN { print "#define TOKENS \\"; print "TOK(kEOF) \\";  } \
#		{ printf "TOK(%s) \\\n", $$2 } \
#		END { print "" }'  ../include/taglparser.h > $@

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
 
#taglscanner.o: taglscanner.yy.h taglscanner.yy.cc
 
#taglscanner.yy.cc: flexfiles
 
#taglscanner.yy.h: flexfiles
 
#.PHONY: flexfiles
#flexfiles: taglscanner.l
#	flex --outfile=taglscanner.yy.cc --header-file=taglscanner.yy.h taglscanner.l
 
.PHONY: clean
clean:
	rm -f *.o
#	rm -f taglscanner.yy.cc taglscanner.yy.h
	rm -f taglparser.cc taglparser.out ../include/taglparser.h 

