MODULE_big = overflow
OBJS = int.o overflow.o uint.o $(WIN32RES)

EXTENSION = overflow
DATA = overflow--1.0.sql
PGFILEDESC = "overflow - checks for overflows"

REGRESS = overflow

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
