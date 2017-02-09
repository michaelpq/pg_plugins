# Global makefile for pg_plugins 

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SUBDIRS = blackhole	\
	compress_test	\
	count_relations	\
	decoder_raw	\
	hello_notify	\
	hello_signal	\
	hello_world	\
	hook_utility	\
	jsonlog		\
	kill_idle	\
	mcxtalloc_test	\
	pg_rep_state	\
	pg_sasl_prepare	\
	pg_wal_blocks	\
	pgmpc		\
	receiver_raw

$(recurse)
$(recurse_always)
