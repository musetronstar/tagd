MAKE_DIRS = tagd tagspace tagl tagsh httagd

DEBUG=

.PHONY: debug build all

all: build

debug: DEBUG = debug
debug: build

build: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir $(DEBUG) ; \
	done

tests: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir tests $(DEBUG) ; \
	done

clean: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir clean ; \
	done

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

