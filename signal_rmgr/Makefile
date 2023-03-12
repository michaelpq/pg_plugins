MODULES = signal_rmgr

EXTENSION = signal_rmgr
DATA = signal_rmgr--1.0.sql
PGFILEDESC = "signal_rmgr - send signals through WAL"

TAP_TESTS = 1

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
