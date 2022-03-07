MODULES = pg_rusage

EXTENSION = pg_rusage
DATA = pg_rusage--1.0.sql
PGFILEDESC = "pg_rusage - CPU measurement"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
