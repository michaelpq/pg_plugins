MODULES = pg_syncrep_state

EXTENSION = pg_syncrep_state
DATA = pg_syncrep_state--1.0.sql
PGFILEDESC = "pg_syncrep_state - fetch backend status for synchronous replication"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
