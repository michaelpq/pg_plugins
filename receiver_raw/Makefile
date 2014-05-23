MODULE_big = receiver_raw
OBJS = receiver_raw.o
SHLIB_LINK = $(libpq)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
