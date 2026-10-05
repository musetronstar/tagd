TAGD_DIR = ../../tagd
TAGSPACE_DIR = ../../tagspace
INC = -I../include -I$(TAGD_DIR)/include -I../../tagspace/include
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
LIBTAGL = ../lib/libtagl.a
LIBTAGSPACE = $(TAGSPACE_DIR)/lib/libtagspace.a

CXXTEST = cxxtestgen --error-printer
# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3 -Wno-write-strings
LFLAGS =  -L $(TAGSPACE_DIR)/lib -ltagspace -L../lib -ltagl -L$(TAGD_DIR)/lib -ltagd -levent -lsqlite3

TESTER = ./tester
TESTER_TAGDURL = ./tagdurl_tester

all: build

debug: CXXFLAGS += -g -O0
debug: all

VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrind: TESTER = $(VALGRIND) ./tester
valgrind: debug

build: tagl_tests tagdurl_tests

tagl_tests: $(LIBTAGD) $(LIBTAGL) $(LIBTAGSPACE)
	$(CXXTEST) Tester.h -o tester.cpp
	g++ $(CXXFLAGS) -o tester tester.cpp $(INC) -Wl,--start-group $(LFLAGS) -Wl,--end-group
	$(TESTER)

tagdurl_tests: $(LIBTAGD) $(LIBTAGL) $(LIBTAGSPACE)
	$(CXXTEST) TagdUrlTester.h -o tagdurl_tester.cpp
	g++ $(CXXFLAGS) -o tagdurl_tester tagdurl_tester.cpp $(INC) -Wl,--start-group $(LFLAGS) -Wl,--end-group
	$(TESTER_TAGDURL)

$(LIBTAGD):
	make -C $(TAGD_DIR)

clean:
	rm -f tester.cpp tester.o ./tester tagdurl_tester.cpp tagdurl_tester.o ./tagdurl_tester
