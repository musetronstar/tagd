INCLUDE = ../include

# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -O3
INC = -I$(INCLUDE)
LFLAGS = -L../lib -ltagd
CXXTEST = cxxtestgen --error-printer

all: tag_tests domain_tests url_tests file_tests

debug: CXXFLAGS += -g -O0
debug: all

TESTER = ./tester
DOMAIN_TESTER = ./domain-tester
URL_TESTER = ./url-tester
FILE_TESTER = ./file-tester

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) ./tester
valgrind: DOMAIN_TESTER = $(VALGRIND) ./domain-tester
valgrind: URL_TESTER = $(VALGRIND) ./url-tester
valgrind: FILE_TESTER = $(VALGRIND) ./file-tester
valgrind: debug

tag_tests:
	$(CXXTEST) Tester.h -o tester.cpp
	g++ $(CXXFLAGS) tester.cpp -o tester $(INC) $(LFLAGS)
	$(TESTER)

domain_tests:
	$(CXXTEST) DomainTester.h -o domain-tester.cpp
	g++ $(CXXFLAGS) domain-tester.cpp -o domain-tester $(INC) $(LFLAGS)
	$(DOMAIN_TESTER)

url_tests:
	$(CXXTEST) UrlTester.h -o url-tester.cpp
	g++ $(CXXFLAGS) url-tester.cpp -o url-tester $(INC) $(LFLAGS)
	$(URL_TESTER)

file_tests:
	$(CXXTEST) FileTester.h -o file-tester.cpp
	g++ $(CXXFLAGS) file-tester.cpp -o file-tester $(INC) $(LFLAGS)
	$(FILE_TESTER)

clean:
	rm -f tester.cpp tester
	rm -f url-tester.cpp url-tester
	rm -f domain-tester.cpp  domain-tester
	rm -f file-tester.cpp file-tester

