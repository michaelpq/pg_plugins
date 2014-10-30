# Global makefile for pg_plugins 

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SUBDIRS = blackhole \
	count_relations \
	decoder_raw \
	hello_notify \
	hello_signal \
	hello_world \
	jsonlog \
	kill_idle \
	receiver_raw

$(recurse)
$(recurse_always)
