MODULES = blackhole_am

EXTENSION = blackhole_am
DATA = blackhole_am--1.0.sql
PGFILEDESC = "blackhole_am - template table AM eating all data"

REGRESS = blackhole_am

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
