PGFILEDESC = "pg_mark_glibc - Mark glibc version in Postgres data folder"
PGAPPICON = win32

PROGRAM = pg_mark_glibc
OBJS = pg_mark_glibc.o $(WIN32RES)

all: check_os pg_mark_glibc

# This rule's only purpose is to make sure that this is only compiled on
# Linux.
.PHONY: check_os
check_os:
ifeq ($(PORTNAME), linux)
	@echo "pg_mark_glibc can only be compiled on Linux."
	@exit 0
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SHLIB_LINK += -lc
