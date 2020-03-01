INCLUDE = ../include

CXX = g++ -std=c++14 -Wall -Wextra
CXXFLAGS = -O3
INC = -I$(INCLUDE)
LFLAGS = -L../lib -ltagd
CXXTEST = cxxtestgen --error-printer

all: tag_tests domain_tests url_tests

debug: CXXFLAGS = -g
debug: all

TESTER = ./tester
URL_TESTER = ./url-tester
DOMAIN_TESTER = ./domain-tester

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) ./tester
valgrind: URL_TESTER = $(VALGRIND) ./url-tester
valgrind: DOMAIN_TESTER = $(VALGRIND) ./domain-tester
valgrind: debug

tag_tests:
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) $(CXXFLAGS) tester.cpp -o tester $(INC) $(LFLAGS)
	$(TESTER)

url_tests:
	$(CXXTEST) UrlTester.h -o url-tester.cpp
	$(CXX) $(CXXFLAGS) url-tester.cpp -o url-tester $(INC) $(LFLAGS)
	$(URL_TESTER)

domain_tests:
	$(CXXTEST) DomainTester.h -o domain-tester.cpp
	$(CXX) $(CXXFLAGS) domain-tester.cpp -o domain-tester $(INC) $(LFLAGS)
	$(DOMAIN_TESTER)

clean:
	rm -f tester.cpp tester.o ./tester
	rm -f url-tester.cpp url-tester.o url-tester
	rm -f domain-tester.cpp domain-tester.o domain-tester

