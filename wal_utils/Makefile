MODULES = wal_utils

EXTENSION = wal_utils
DATA = wal_utils--1.0.sql
PGFILEDESC = "wal_utils - Set of tools for WAL data"
REGRESS = parse_wal_history wal_segment_list

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
