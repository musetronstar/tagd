CXX= g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3

GPERF=public-suffix.gperf
GPERF_H=public-suffix.gperf.h
OBJS=domain.o

INC_DIR=../../include

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build: $(OBJS)

$(OBJS): $(GPERF_H) $(INC_DIR)/tagd/domain.h domain.cc
	$(CXX) $(CXXFLAGS) -Wno-missing-field-initializers -c domain.cc -I $(INC_DIR) -I./

$(GPERF_H): $(GPERF)
	gperf $? > $@

$(GPERF): effective_tld_names.dat
	./gen-gperf-tlds.pl $? > $@

tests: all force_look
	make -C tests $(DEBUG)

force_look:
	true

clean:
	rm -f *.o
	make -C tests clean

extra_clean: clean
	rm -f $(GPERF) $(GPERF_H)
