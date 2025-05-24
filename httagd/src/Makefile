# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -Wall -Wextra -fpermissive -O3

PRG_DIR = ../..
TAGD_DIR = $(PRG_DIR)/tagd
TAGL_DIR = $(PRG_DIR)/tagl
TAGSPACE_DIR = $(PRG_DIR)/tagdb
TAGSH_DIR = $(PRG_DIR)/tagsh

BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/httagd

INC = -I../include \
	  -I$(TAGD_DIR)/include \
	  -I$(TAGL_DIR)/include \
	  -I$(TAGSPACE_DIR)/include \
	  -I$(TAGSPACE_DIR)/sqlite/include \
	  -I$(TAGSH_DIR)/include

LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagdb-sqlite \
		 -L $(TAGL_DIR)/lib -ltagl \
		 -L $(TAGD_DIR)/lib -ltagd \
		 -L $(TAGSH_DIR)/lib -ltagsh \
		 -lsqlite3 -levent -lreadline

ifdef EVHTP_SRC_DIR
	INC += -I$(EVHTP_SRC_DIR)/include -I$(EVHTP_SRC_DIR)/build/include
	EVHTP_H = $(EVHTP_SRC_DIR)/include/evhtp/evhtp.h
else
	INC += -I/usr/include/evhtp
	EVHTP_H = /usr/include/evhtp/evhtp.h
endif

all: build

debug: CXXFLAGS += -g -O0
debug: build

build : httagd.o

# token to string case statements
evhtp-results.inc: $(EVHTP_H)
	grep '#define EVHTP_RES_[A-Z]' $? | awk '{printf "case %s: return \"%s\";\n", $$2, $$2;}' > $@

httagd.cc: evhtp-results.inc

httagd.o : httagd.cc
	g++ $(CXXFLAGS) -c -o $@ $? $(INC) $(LIBEVHTP_INC)

clean :
	rm -f *.o
	rm -f *.inc
	rm -f $(BIN)

