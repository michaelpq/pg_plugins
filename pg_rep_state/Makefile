MODULES = pg_rep_state

EXTENSION = pg_rep_state
DATA = pg_rep_state--1.0.sql
PGFILEDESC = "pg_rep_state - fetch replication status of server"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
