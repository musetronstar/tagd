# Use flags from top-level, or defaults if called directly
CXXFLAGS ?= -std=c++23 -Wall -Wextra -O3

PRG_DIR = ../..
TAGD_DIR = $(PRG_DIR)/tagd
TAGL_DIR = $(PRG_DIR)/tagl
TAGSPACE_DIR = $(PRG_DIR)/tagdb
TAGSH_DIR = $(PRG_DIR)/tagsh
HTTAGD_DIR = $(PRG_DIR)/httagd

BIN_DIR = ../bin
BIN = $(BIN_DIR)/httagd

LIBHTTAGD = $(HTTAGD_DIR)/lib/libhttagd.a
LIBTAGD = $(TAGD_DIR)/lib/libtagd.a
LIBTAGL = $(TAGL_DIR)/lib/libtagl.a
LIBTAGDB_SQLITE = $(TAGSPACE_DIR)/lib/libtagdb-sqlite.a
LIBTAGSH = $(TAGSH_DIR)/lib/libtagsh.a

INC = -I$(TAGD_DIR)/include \
	  -I$(TAGSPACE_DIR)/include \
	  -I$(TAGSPACE_DIR)/sqlite/include \
	  -I$(TAGL_DIR)/include \
	  -I$(TAGSH_DIR)/include \
	  -I${HTTAGD_DIR}/include

# WTF why must httagd.a be linked first, or else 'undefined reference' errors?
LFLAGS = -L $(HTTAGD_DIR)/lib -lhttagd \
		 -L $(TAGSPACE_DIR)/lib -ltagdb-sqlite \
		 -L $(TAGL_DIR)/lib -ltagl \
		 -L $(TAGSH_DIR)/lib -ltagsh \
		 -L $(TAGD_DIR)/lib -ltagd \
		 -lsqlite3 -levent -lreadline -lctemplate_nothreads

ifdef EVHTP_SRC_DIR
	INC += -I$(EVHTP_SRC_DIR)/include -I$(EVHTP_SRC_DIR)/build/include
	LFLAGS += -L$(EVHTP_SRC_DIR)/build
else
	INC += -I/usr/include/evhtp
endif

LFLAGS += -levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lpthread -ldl -lrt

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS += -g -O0
debug: build

DEPS = main.d

build : $(BIN)

main.o : main.cc
	g++ $(CXXFLAGS) -MMD -MP -c -o $@ $< $(INC)

$(BIN) : main.o $(LIBHTTAGD) $(LIBTAGD) $(LIBTAGL) $(LIBTAGDB_SQLITE) $(LIBTAGSH)
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	g++ $(CXXFLAGS) -o $@ main.o $(LFLAGS)

-include $(DEPS)

clean :
	rm -f *.o *.d
	rm -f $(BIN)
