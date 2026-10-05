SHELL_TESTER = ./tester.exp
LOG_UNIT_TESTER = ./log-tester
DRIVER_UNIT_TESTER = ./driver-tester
BIN = ../bin/tagsh --noshell --file bootstrap.tagl
VALGRIND = valgrind --leak-check=full --error-exitcode=1
CXXTEST = cxxtestgen --error-printer
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3 -Wno-write-strings
INC = -I../include -I../../tagd/include -I../../tagl/include -I../../tagspace/include
LFLAGS = -L../lib -ltagsh -L../../tagspace/lib -ltagspace -L../../tagl/lib -ltagl -L../../tagd/lib -ltagd -lsqlite3 -levent -lreadline

all: tests

tests:
	$(MAKE) unit_tests
	$(MAKE) shell_tests

unit_tests:
	$(MAKE) driver_unit_tests
	$(MAKE) log_unit_tests

shell_tests:
	$(SHELL_TESTER)

system_tests: shell_tests
expect_tests: shell_tests

driver_tester:
	$(MAKE) driver_unit_tests

driver_unit_tests: $(DRIVER_UNIT_TESTER)
	$(DRIVER_UNIT_TESTER)

log_tester:
	$(MAKE) log_unit_tests

log_unit_tests: $(LOG_UNIT_TESTER)
	$(LOG_UNIT_TESTER)

$(DRIVER_UNIT_TESTER): DriverTester.h ../lib/libtagsh.a ../../tagspace/lib/libtagspace.a ../../tagl/lib/libtagl.a ../../tagd/lib/libtagd.a
	$(CXXTEST) DriverTester.h -o driver-tester.cpp
	g++ $(CXXFLAGS) driver-tester.cpp -o driver-tester $(INC) -Wl,--start-group $(LFLAGS) -Wl,--end-group

$(LOG_UNIT_TESTER): LogTester.h ../lib/libtagsh.a ../../tagspace/lib/libtagspace.a ../../tagl/lib/libtagl.a ../../tagd/lib/libtagd.a
	$(CXXTEST) LogTester.h -o log-tester.cpp
	g++ $(CXXFLAGS) log-tester.cpp -o log-tester $(INC) -Wl,--start-group $(LFLAGS) -Wl,--end-group

valgrind:
	$(VALGRIND) $(BIN)
