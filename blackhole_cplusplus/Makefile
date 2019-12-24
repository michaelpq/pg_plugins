MODULE_big = blackhole_cplusplus

EXTENSION = blackhole_cplusplus
CXX_SRCS = blackhole_cplusplus.cc
DATA = blackhole_cplusplus--1.0.sql
OBJS = $(CXX_SRCS:.cc=.o)
PGFILEDESC = "blackhole_cplusplus - Minimal extension template in C++"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
