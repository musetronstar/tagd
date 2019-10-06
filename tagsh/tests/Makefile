TESTER = ./tester.exp
BIN = ../bin/tagsh --noshell --file ../bootstrap.tagl
VALGRIND = valgrind --leak-check=full --error-exitcode=1

all: tests

tests:
	$(TESTER)

valgrind:
	$(VALGRIND) $(BIN)
