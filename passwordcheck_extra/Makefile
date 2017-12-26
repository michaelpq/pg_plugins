# passwordcheck_extra/Makefile

MODULE_big = passwordcheck_extra
OBJS = passwordcheck_extra.o $(WIN32RES)
PGFILEDESC = "passwordcheck_extra - strengthen user password checks"

REGRESS = passwordcheck_extra

# uncomment the following two lines to enable cracklib support
# PG_CPPFLAGS = -DUSE_CRACKLIB '-DCRACKLIB_DICTPATH="/usr/lib/cracklib_dict"'
# SHLIB_LINK = -lcrack

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
