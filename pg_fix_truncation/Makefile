MODULES = pg_fix_truncation

EXTENSION = pg_fix_truncation
DATA = pg_fix_truncation--1.0.sql
PGFILEDESC = "pg_fix_truncation - Fix truncation of corrupted FSM files"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
