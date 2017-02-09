MODULES = pg_sasl_prepare

EXTENSION = pg_sasl_prepare
DATA = pg_sasl_prepare--1.0.sql
PGFILEDESC = "pg_sasl_prepare - SASLprepare for UTF-8 strings"

DOWNLOAD = wget -O $@ --no-use-server-timestamps

all: utf8_table pg_sasl_prepare.so

UnicodeData.txt:
	$(DOWNLOAD) http://unicode.org/Public/UNIDATA/UnicodeData.txt

utf8_table: UnicodeData.txt
	$(PERL) generate_conv.pl UnicodeData.txt utf8_table.h

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
