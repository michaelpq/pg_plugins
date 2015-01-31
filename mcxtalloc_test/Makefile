MODULES = mcxtalloc_test

EXTENSION = mcxtalloc_test
DATA = mcxtalloc_test--1.0.sql
PGFILEDESC = "mcxtalloc_test - Test low-level allocation functions"
REGRESS = mcxtalloc_test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
