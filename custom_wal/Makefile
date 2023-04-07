MODULES = custom_wal

EXTENSION = custom_wal
DATA = custom_wal--1.0.sql
PGFILEDESC = "custom_wal - Generate custom WAL records"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
