INCLUDE = ../include

# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
INC = -I$(INCLUDE)
LIBTAGD = ../lib/libtagd.a
LFLAGS = -L../lib -ltagd
CXXTEST = cxxtestgen --error-printer

all: tag_tests domain_tests url_tests event_tests file_tests logger_tests hard_tag_tests

debug: CXXFLAGS += -g -O0
debug: all

TESTER = ./tester
DOMAIN_TESTER = ./domain-tester
URL_TESTER = ./url-tester
EVENT_TESTER = ./event-tester
FILE_TESTER = ./file-tester
LOGGER_TESTER = ./logger-tester
HARD_TAG_TESTER = ./hard-tag-tester

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) ./tester
valgrind: DOMAIN_TESTER = $(VALGRIND) ./domain-tester
valgrind: URL_TESTER = $(VALGRIND) ./url-tester
valgrind: EVENT_TESTER = $(VALGRIND) ./event-tester
valgrind: FILE_TESTER = $(VALGRIND) ./file-tester
valgrind: LOGGER_TESTER = $(VALGRIND) ./logger-tester
valgrind: HARD_TAG_TESTER = $(VALGRIND) ./hard-tag-tester
valgrind: debug

tag_tests: $(LIBTAGD) Tester.h
	$(CXXTEST) Tester.h -o tester.cpp
	g++ $(CXXFLAGS) tester.cpp -o tester $(INC) $(LFLAGS)
	$(TESTER)

domain_tests: $(LIBTAGD) DomainTester.h
	$(CXXTEST) DomainTester.h -o domain-tester.cpp
	g++ $(CXXFLAGS) domain-tester.cpp -o domain-tester $(INC) $(LFLAGS)
	$(DOMAIN_TESTER)

url_tests: $(LIBTAGD) UrlTester.h
	$(CXXTEST) UrlTester.h -o url-tester.cpp
	g++ $(CXXFLAGS) url-tester.cpp -o url-tester $(INC) $(LFLAGS)
	$(URL_TESTER)

event_tests: $(LIBTAGD) EventTester.h
	$(CXXTEST) EventTester.h -o event-tester.cpp
	g++ $(CXXFLAGS) event-tester.cpp -o event-tester $(INC) $(LFLAGS)
	$(EVENT_TESTER)

file_tests: $(LIBTAGD) FileTester.h
	$(CXXTEST) FileTester.h -o file-tester.cpp
	g++ $(CXXFLAGS) file-tester.cpp -o file-tester $(INC) $(LFLAGS)
	$(FILE_TESTER)

logger_tests: $(LIBTAGD) LoggerTester.h
	$(CXXTEST) LoggerTester.h -o logger-tester.cpp
	g++ $(CXXFLAGS) logger-tester.cpp -o logger-tester $(INC) $(LFLAGS)
	$(LOGGER_TESTER)

hard_tag_tests: $(LIBTAGD) HardTagTester.h
	$(CXXTEST) HardTagTester.h -o hard-tag-tester.cpp
	g++ $(CXXFLAGS) hard-tag-tester.cpp -o hard-tag-tester $(INC) $(LFLAGS)
	$(HARD_TAG_TESTER)

clean:
	rm -f tester.cpp tester
	rm -f url-tester.cpp url-tester
	rm -f event-tester.cpp event-tester
	rm -f domain-tester.cpp  domain-tester
	rm -f file-tester.cpp file-tester
	rm -f logger-tester.cpp logger-tester
	rm -f hard-tag-tester.cpp hard-tag-tester
