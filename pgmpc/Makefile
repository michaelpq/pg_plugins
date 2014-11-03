MODULE_big = pgmpc
OBJS    = pgmpc.o $(WIN32RES)

EXTENSION = pgmpc
DATA = pgmpc--1.0.sql
PGFILEDESC = "pgmpc - mpd client for PostgreSQL"
SHLIB_LINK = -lmpdclient

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
