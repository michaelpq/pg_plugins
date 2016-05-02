MODULES = pg_swap_pages

EXTENSION = pg_swap_pages
DATA = pg_swap_pages--1.0.sql
PGFILEDESC = "pg_swap_pages - Swap relation pages at WAL replay"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
