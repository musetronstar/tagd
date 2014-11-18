CXX = g++ -std=c++0x -Wall -Wextra
CXXFLAGS = -O3

PRG_DIR = ../..
TAGD_DIR = $(PRG_DIR)/tagd
TAGL_DIR = $(PRG_DIR)/tagl
TAGSPACE_DIR = $(PRG_DIR)/tagspace
TAGSH_DIR = $(PRG_DIR)/tagsh

BUILD_DIR = ../bin
BIN = $(BUILD_DIR)/httagd

INC = -I../include -I$(TAGD_DIR)/include -I$(TAGL_DIR)/include -I$(TAGSPACE_DIR)/include -I$(TAGSPACE_DIR)/sqlite/include -I$(TAGSH_DIR)/include
LFLAGS = -L $(TAGSPACE_DIR)/lib -ltagspace-sqlite -L $(TAGL_DIR)/lib -ltagl -L $(TAGD_DIR)/lib -ltagd -L $(TAGSH_DIR)/lib -ltagsh -lsqlite3 -levent -lreadline

LIBEVHTP_DIR = $(HOME)/src/libevhtp
LIBEVHTP_INC = -I$(LIBEVHTP_DIR) -I$(LIBEVHTP_DIR)/build -I$(LIBEVHTP_DIR)/build/compat -I$(LIBEVHTP_DIR)/htparse -I$(LIBEVHTP_DIR)/build/oniguruma -I$(LIBEVHTP_DIR)/oniguruma -I$(LIBEVHTP_DIR)/evthr
LIBEVHTP_LFLAGS = -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lpthread -ldl -lrt
LIBEVHTP = $(LIBEVHTP_DIR)/build/libevhtp.a

all: build

DEBUG=
debug: DEBUG=debug
debug: CXXFLAGS = -g
debug: build

build : $(BIN)

main.o : main.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $? $(INC) $(LIBEVHTP_INC)

# token to string case statements
evhtp-results.inc: $(LIBEVHTP_DIR)/evhtp.h
	grep '#define EVHTP_RES_[A-Z]' $? | awk '{printf "case %s: return \"%s\";\n", $$2, $$2;}' > $@

httagd.o : evhtp-results.inc httagd.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $? $(INC) $(LIBEVHTP_INC)
 
template_callback.o : template_callback.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $? $(INC) $(LIBEVHTP_INC)

$(BIN) : main.o httagd.o template_callback.o
	$(CXX) $(CXXFLAGS) -o $@ $? $(LFLAGS) $(LIBEVHTP) $(LIBEVHTP_LFLAGS) -lctemplate_nothreads

clean :
	rm -f *.o
	rm -f *.inc
	rm -f $(BIN)
