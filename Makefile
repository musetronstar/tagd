MAKE_DIRS = tagd tagspace tagl tagsh

all: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir ; \
	done

tests: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir tests; \
	done

clean: force_look
	for dir in $(MAKE_DIRS) ; do \
		make -C $$dir clean ; \
	done

# 'true' forces make to look (otherwise its always up to date)
force_look:
	true

