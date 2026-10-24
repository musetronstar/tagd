include ../../backend.mk

INCLUDE_TAGSPACE = ../../include
INCLUDE_TAGD = ../../../tagd/include

CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
INC = -I../include -I$(INCLUDE_TAGSPACE) -I$(INCLUDE_TAGD) $(TAGSPACE_CPPFLAGS)
LIBTAGD = ../../../tagd/lib/libtagd.a
LIBTAGSPACE = ../../lib/libtagspace.a
LFLAGS = -L../../lib -ltagspace -L../../../tagd/lib -ltagd $(TAGSPACE_BACKEND_LIBS)
CXXTEST = cxxtestgen --error-printer

all: filepile_tests filepile_files_tests filepile_vocabulary_tests

debug: CXXFLAGS += -g -O0
debug: all

filepile_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileTester.h
	$(CXXTEST) FilepileTester.h -o filepile-tester.cpp
	g++ $(CXXFLAGS) filepile-tester.cpp -o filepile-tester $(INC) $(LFLAGS) -Wl,--wrap=sqlite3_exec,--wrap=read,--wrap=unlinkat
	./filepile-tester

filepile_files_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileFilesTester.h
	$(CXXTEST) FilepileFilesTester.h -o filepile-files-tester.cpp
	g++ $(CXXFLAGS) filepile-files-tester.cpp -o filepile-files-tester $(INC) $(LFLAGS) -Wl,--wrap=read,--wrap=write,--wrap=fsync,--wrap=linkat,--wrap=unlinkat
	./filepile-files-tester

filepile_vocabulary_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileVocabularyTester.h
	$(CXXTEST) FilepileVocabularyTester.h -o filepile_vocabulary_tests-tester.cpp
	g++ $(CXXFLAGS) filepile_vocabulary_tests-tester.cpp -o filepile_vocabulary_tests-tester $(INC) $(LFLAGS)
	./filepile_vocabulary_tests-tester

$(LIBTAGSPACE):
	$(MAKE) -C ../.. libtagspace CXXFLAGS="$(CXXFLAGS)"

$(LIBTAGD):
	$(MAKE) -C ../../../tagd CXXFLAGS="$(CXXFLAGS)"

clean:
	rm -f filepile-tester.cpp filepile-tester
	rm -f filepile-files-tester.cpp filepile-files-tester
	rm -f filepile_vocabulary_tests-tester.cpp filepile_vocabulary_tests-tester
