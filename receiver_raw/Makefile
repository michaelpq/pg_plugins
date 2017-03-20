MODULE_big = receiver_raw
OBJS = receiver_raw.o

PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK = $(libpq)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
+CC+=-I $(shell $(PG_CONFIG) --includedir)

