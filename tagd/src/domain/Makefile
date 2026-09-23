# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

GPERF=public-suffix.gperf
GPERF_H=public-suffix.gperf.h
OBJS=domain.o
DEPS=$(OBJS:.o=.d)

INC_DIR=../../include

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS += -g -O0
debug: build

build: $(OBJS)

$(OBJS): $(GPERF_H) $(INC_DIR)/tagd/domain.h domain.cc
	g++ $(CXXFLAGS) -MMD -MP -Wno-missing-field-initializers -c domain.cc -I $(INC_DIR) -I./

$(GPERF_H): $(GPERF)
	gperf $? > $@

effective_tld_names.dat:
	wget https://publicsuffix.org/list/effective_tld_names.dat -O $@

$(GPERF): effective_tld_names.dat
	./gen-gperf-tlds.pl $? > $@

-include $(DEPS)

clean:
	rm -f *.o *.d

distclean: clean
	rm -f $(GPERF) $(GPERF_H) effective_tld_names.dat
