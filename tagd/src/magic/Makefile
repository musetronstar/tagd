CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

# Project libraries report errors explicitly; tests retain their own compiler flags.
override CXXFLAGS += -fno-exceptions

all: magic.o

# Publish generator outputs only after validation succeeds, preserving the last good build.
media-types.gperf: media-types.csv canonical-extensions.csv gen-gperf-media-types.pl
	./gen-gperf-media-types.pl media-types.csv canonical-extensions.csv > $@.tmp && mv $@.tmp $@

media-types.gperf.h: media-types.gperf
	gperf $< > $@.tmp && mv $@.tmp $@

magic.o: magic.cc media-types.gperf.h ../../include/tagd/magic.h
	$(CXX) $(CXXFLAGS) -Wno-missing-field-initializers -MMD -MP -I../../include -c magic.cc -o $@

-include magic.d

debug: all
clean:
	rm -f magic.o magic.d *.tmp
distclean: clean
	rm -f media-types.gperf media-types.gperf.h
