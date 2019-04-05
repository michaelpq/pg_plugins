PGFILEDESC = "pg_mark_glibc - Mark glibc version in Postgres data folder"
PGAPPICON = win32

PROGRAM = pg_mark_glibc
OBJS = pg_mark_glibc.o $(WIN32RES)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SHLIB_LINK += -lc
