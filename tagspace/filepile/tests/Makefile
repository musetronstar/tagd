include ../../backend.mk

INCLUDE_TAGSPACE = ../../include
INCLUDE_TAGD = ../../../tagd/include

CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3
INC = -I../include -I$(INCLUDE_TAGSPACE) -I$(INCLUDE_TAGD) $(TAGSPACE_CPPFLAGS)
LIBTAGD = ../../../tagd/lib/libtagd.a
LIBTAGSPACE = ../../lib/libtagspace.a
LFLAGS = -L../../lib -ltagspace -L../../../tagd/lib -ltagd $(TAGSPACE_BACKEND_LIBS)
CXXTEST = cxxtestgen --error-printer

all: filepile_tests filepile_files_tests filepile_vocabulary_tests filepile_command_tests filepile_delete_tests

filepile_delete_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileDeleteTester.h
	$(CXXTEST) FilepileDeleteTester.h -o filepile-delete-tester.cpp
	g++ $(CXXFLAGS) filepile-delete-tester.cpp -o filepile-delete-tester $(INC) $(LFLAGS) -Wl,--wrap=sqlite3_exec,--wrap=unlinkat,--wrap=fsync,--wrap=sqlite3_step
	./filepile-delete-tester

filepile_command_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileCommandTester.h
	$(CXXTEST) FilepileCommandTester.h -o filepile-command-tester.cpp
	g++ $(CXXFLAGS) filepile-command-tester.cpp -o filepile-command-tester $(INC) $(LFLAGS) -Wl,--wrap=sqlite3_step
	./filepile-command-tester

debug: CXXFLAGS += -g -O0
debug: all

filepile_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileTester.h
	$(CXXTEST) FilepileTester.h -o filepile-tester.cpp
	g++ $(CXXFLAGS) filepile-tester.cpp -o filepile-tester $(INC) $(LFLAGS) -Wl,--wrap=sqlite3_exec,--wrap=read,--wrap=unlinkat
	./filepile-tester

filepile_files_tests: $(LIBTAGSPACE) $(LIBTAGD) FilepileFilesTester.h
	$(CXXTEST) FilepileFilesTester.h -o filepile-files-tester.cpp
	g++ $(CXXFLAGS) filepile-files-tester.cpp -o filepile-files-tester $(INC) $(LFLAGS) -Wl,--wrap=read,--wrap=write,--wrap=fsync,--wrap=linkat,--wrap=unlinkat,--wrap=readdir
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
	rm -f filepile-delete-tester.cpp filepile-delete-tester
	rm -f filepile-command-tester.cpp filepile-command-tester
	rm -f filepile-tester.cpp filepile-tester
	rm -f filepile-files-tester.cpp filepile-files-tester
	rm -f filepile_vocabulary_tests-tester.cpp filepile_vocabulary_tests-tester
