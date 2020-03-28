-- Set of SQL functions to emulate updates on num_tables tables
-- filled in num_tuples tuples.
-- This can be used with pgbench in two steps:
-- 1) Initialize the tables to use, here 10 tables filled in with
--    1000 tuples each:
-- SELECT create_tables(10, 1000);
-- 2) Use the following script to trigger random updates on each
--    tuple of each table:
-- \set num_table random(1, 10)
-- \set num_tuple random(1, 1000)
-- \set val random(-2000, 2000)
-- SELECT update_table(:num_table, :num_tuple, :val);
--
-- This can then be invoked with a command like that:
-- pgbench -c 24 -f update_script.sql -T 3600

CREATE OR REPLACE FUNCTION create_tables(num_tables int, num_tuples int)
RETURNS VOID AS
$func$
DECLARE
  tabname text;
BEGIN
  FOR i IN 1..num_tables LOOP
    tabname = 't_' || i;
    EXECUTE format('
      CREATE TABLE IF NOT EXISTS %I (id int, id2 int)', tabname);
    EXECUTE format('
      ALTER TABLE %I SET (autovacuum_freeze_table_age = 1000000)', tabname);
    EXECUTE format('
      INSERT INTO %I VALUES (generate_series(1, %L), 0)', tabname, num_tuples);
  END LOOP;
END
$func$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION update_table(num_table int, num_tuple int, val int)
RETURNS VOID AS
$func$
DECLARE
  tabname text;
BEGIN
  tabname = 't_' || num_table;
  EXECUTE format('
    UPDATE %I SET id2 = id2 + %L::int4 WHERE id = %L', tabname, val, num_tuple);
END
$func$ LANGUAGE plpgsql;
