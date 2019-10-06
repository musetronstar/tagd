TARGET=
MAKE_DIRS = tagd tagdb tagl tagsh httagd

debug: TARGET=debug
debug: build

valgrind: TARGET=valgrind
valgrind: build

tests: TARGET=tests
tests: build

clean: TARGET=clean
clean: build

all: build

build: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir $(TARGET) ; \
	done

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

