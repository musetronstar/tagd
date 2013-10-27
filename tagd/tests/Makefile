INCLUDE = ../include

CXX = g++ -std=c++0x 
CXXTEST=~/src/cxxtest/bin/cxxtestgen --error-printer
CFLAGS = -DDEBUG -ggdb -I ~/src/cxxtest -I$(INCLUDE)
LFLAGS = -L../lib -ltagd $(PG)

all: tag_tests url_tests domain_tests

tag_tests:
	$(CXXTEST) Tester.h -o tester.cpp
	$(CXX) tester.cpp -o tester $(CFLAGS) $(LFLAGS)
	./tester

url_tests:
	$(CXXTEST) UrlTester.h -o url-tester.cpp
	$(CXX) url-tester.cpp -o url-tester $(CFLAGS) $(LFLAGS)
	./url-tester

domain_tests:
	$(CXXTEST) DomainTester.h -o domain-tester.cpp
	$(CXX) domain-tester.cpp -o domain-tester $(CFLAGS) $(LFLAGS)
	./domain-tester

clean:
	rm -f tester.cpp tester.o ./tester
	rm -f url-tester.cpp url-tester.o url-tester
	rm -f domain-tester.cpp domain-tester.o domain-tester

