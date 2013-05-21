CXX=g++

GPERF=public-suffix.gperf
GPERF_H=public-suffix.gperf.h
OBJS=domain.o

INC_DIR=../../include

all: $(OBJS)

$(OBJS): $(GPERF_H) $(INC_DIR)/tagd/domain.h domain.cc
	$(CXX) -c domain.cc -I $(INC_DIR) -I./

$(GPERF_H): $(GPERF)
	gperf $? > $@

$(GPERF): effective_tld_names.dat
	./gen-gperf-tlds.pl $? > $@

tests: all force_look
	make -C tests

force_look:
	true

clean:
	rm -f *.o
	make -C tests clean

extra_clean: clean
	rm -f $(GPERF) $(GPERF_H)
