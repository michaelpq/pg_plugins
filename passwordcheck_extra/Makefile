# passwordcheck_extra/Makefile

MODULE_big = passwordcheck_extra
OBJS = passwordcheck_extra.o $(WIN32RES)
PGFILEDESC = "passwordcheck_extra - strengthen user password checks"

REGRESS = passwordcheck_extra

# uncomment the following two lines to enable cracklib support
# PG_CPPFLAGS = -DUSE_CRACKLIB '-DCRACKLIB_DICTPATH="/usr/lib/cracklib_dict"'
# SHLIB_LINK = -lcrack

PG_CONFIG = pg_config
PG_MAJOR_VERSION := $(word 2, $(subst ., ,$(shell $(PG_CONFIG) --version)))
ifeq ($(PG_MAJOR_VERSION),9)
  PG_CPPFLAGS = -DOLD_PASSWORD_TYPE
endif
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
