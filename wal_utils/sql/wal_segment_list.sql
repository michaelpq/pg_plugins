-- Install extension for tests
CREATE EXTENSION wal_utils;

CREATE TABLE history_data (data text);

-- Load history file to use as a base with multiple timelines
INSERT INTO history_data VALUES (
'1       0/09D4F390      no recovery target specified
2       0/117BEB70      no recovery target specified
3       0/187BEB38      no recovery target specified
4       0/188BEB38      no recovery target specified
5       0/189BEB38      no recovery target specified
7       0/249BEB38      no recovery target specified
');
SELECT parse_wal_history(data) FROM history_data;

-- Build full list of segments needed for recovery.
SELECT build_wal_segment_list(1, '0/06D4F389'::pg_lsn, 8, '0/259BEB38'::pg_lsn, data)
   FROM history_data;
-- Partial list of segments
SELECT build_wal_segment_list(3, '0/177BEB38'::pg_lsn, 8, '0/259BEB38'::pg_lsn, data)
   FROM history_data;
-- error, target TLI older than origin TLI
SELECT build_wal_segment_list(2, '0/09D4F389'::pg_lsn, 1, '0/259BEB38'::pg_lsn, data)
   FROM history_data;
-- error, target LSN older than origin LSN
SELECT build_wal_segment_list(1, '0/09D4F389'::pg_lsn, 2, '0/08D4F389'::pg_lsn, data)
   FROM history_data;
-- error, target LSN older than last history file entry
SELECT build_wal_segment_list(1, '0/09D4F389'::pg_lsn, 2, '0/10D4F389'::pg_lsn, data)
   FROM history_data;
-- error, timelines are not direct parents
SELECT build_wal_segment_list(6, '0/09D4F389'::pg_lsn, 9, '0/259BEB38'::pg_lsn, data)
   FROM history_data;

DROP TABLE history_data;
