PGFILEDESC = "pg_wal_blocks - Tracker of relation blocks changed by WAL records"
PGAPPICON = win32

PROGRAM = pg_wal_blocks
OBJS	= pg_wal_blocks.o xlogreader.o

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

override CPPFLAGS := -DFRONTEND $(CPPFLAGS)

EXTRA_CLEAN = $(RMGRDESCSOURCES) xlogreader.c

all: pg_wal_blocks checksrcdir

# This rule's only purpose is to give the user instructions on how to pass
# the path to PostgreSQL source tree to the makefile.
.PHONY: checksrcdir
checksrcdir:
ifndef top_srcdir
	@echo "You must have PostgreSQL source tree available to compile."
	@echo "Pass the path to the PostgreSQL source tree to make, in the top_srcdir"
	@echo "variable: \"make top_srcdir=<path to PostgreSQL source tree>\""
	@exit 1
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# xlogreader.c is symlinked from the PostgreSQL sources.
xlogreader.c: % : $(top_srcdir)/src/backend/access/transam/%
	rm -f $@ && $(LN_S) $< .
