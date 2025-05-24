MAKE_DIRS = tagd tagdb tagl tagsh httagd
TARGET=
CXXFLAGS = -std=c++23 -Wall -Wextra -Wno-unused-result -O3

all: build

debug: TARGET=debug
debug: CXXFLAGS += -g -DDEBUG -O0
debug: build

valgrind: TARGET=valgrind
valgrind: CXXFLAGS += -g -O0
valgrind: build

tests: TARGET=tests
tests: build

clean: TARGET=clean
clean: build

build: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir $(TARGET) CXXFLAGS="$(CXXFLAGS)" ; \
	done

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

