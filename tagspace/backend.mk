# Select one compiled backend; its factory and link requirements travel together.
TAGSPACE_CPPFLAGS := -I$(dir $(lastword $(MAKEFILE_LIST)))tagdb/include
TAGDB_BACKEND ?= sqlite
TAGDB_LIBRARY = tagdb-$(TAGDB_BACKEND)
include $(dir $(lastword $(MAKEFILE_LIST)))tagdb/$(TAGDB_BACKEND)/backend.mk
