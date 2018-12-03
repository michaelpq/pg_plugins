PGFILEDESC = "pg_checksums - handle checksums in an offline cluster"
PGAPPICON = win32

PROGRAM = pg_checksums
OBJS = pg_checksums.o $(WIN32RES)

TAP_TESTS=1

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
