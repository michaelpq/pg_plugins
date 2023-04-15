MODULES = scram_utils

EXTENSION = scram_utils
DATA = scram_utils--1.0.sql
PGFILEDESC = "scram_utils - Utilities for SCRAM authentication"

REGRESS = scram_utils

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
