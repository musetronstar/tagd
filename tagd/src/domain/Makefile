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

effective_tld_names.dat :
	wget https://mxr.mozilla.org/mozilla-central/source/netwerk/dns/effective_tld_names.dat?raw=1 -O effective_tld_names.dat

$(GPERF): effective_tld_names.dat
	./gen-gperf-tlds.pl $? > $@

force_look:
	true

clean:
	rm -f *.o

extra_clean: clean
	rm -f $(GPERF) $(GPERF_H)
