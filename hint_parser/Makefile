MODULE_big = hint_parser

OBJS = \
	$(WIN32RES) \
	hint_gram.o \
	hint_scanner.o \
	hint_parser.o

REGRESS = hint_parser

EXTENSION = hint_parser
DATA = hint_parser--1.0.sql
PGFILEDESC = "hint_parser - Parser for query hints"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

hint_gram.h: hint_gram.c
	touch $@

hint_gram.c: BISONFLAGS += -d

hint_gram.o hint_scanner.o: hint_gram.h
