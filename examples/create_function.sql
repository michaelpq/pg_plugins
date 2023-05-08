-- functions

-- Template for plpgsql function
CREATE OR REPLACE FUNCTION foo_function(data1 text) RETURNS text AS $$
DECLARE
  res text;
BEGIN
  RAISE NOTICE 'owned';
  SELECT data1::text INTO res;
  RETURN res;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Create many tables at once
CREATE OR REPLACE FUNCTION create_tables(num_tables int)
  RETURNS VOID AS
  $func$
  BEGIN
  FOR i IN 1..num_tables LOOP
    EXECUTE format('
      CREATE TABLE IF NOT EXISTS %I (id int)', 't_' || i);
  END LOOP;
END
$func$ LANGUAGE plpgsql;

-- drop many tables at once, to be used in pair with create_tables()
CREATE OR REPLACE FUNCTION drop_tables(num_tables int)
  RETURNS VOID AS
  $func$
  BEGIN
  FOR i IN 1..num_tables LOOP
    EXECUTE format('
      DROP TABLE IF EXISTS %I', 't_' || i);
  END LOOP;
END
$func$ LANGUAGE plpgsql;

-- Create one table with many columns at once
CREATE OR REPLACE FUNCTION create_table_cols(tabname text, num_cols int)
RETURNS VOID AS
$func$
DECLARE
  query text;
BEGIN
  query := 'CREATE TABLE ' || tabname || ' (';
  FOR i IN 1..num_cols LOOP
    query := query || 'a_' || i::text || ' int';
    IF i != num_cols THEN
      query := query || ', ';
    END IF;
  END LOOP;
  query := query || ')';
  EXECUTE format(query);
END
$func$ LANGUAGE plpgsql;

-- Create one index with many expressions at once
CREATE OR REPLACE FUNCTION create_index_exprs(indname text,
  tabname text,
  num_cols int)
RETURNS VOID AS
$func$
DECLARE
  query text;
BEGIN
  query := 'CREATE INDEX ' || indname || ' ON ' || tabname || ' (';
  FOR i IN 1..num_cols LOOP
    query := query || '(' || 'a_' || i::text || ' / 1)';
    IF i != num_cols THEN
      query := query || ', ';
    END IF;
  END LOOP;
  query := query || ')';
  EXECUTE format(query);
END
$func$ LANGUAGE plpgsql;

-- Create many indexes at once with expressions
CREATE OR REPLACE FUNCTION create_index_multi_exprs(indname text,
  num_inds int, tabname text, num_cols int)
RETURNS VOID AS
$func$
DECLARE
  query text;
BEGIN
  FOR i IN 1..num_inds LOOP
    query := 'SELECT create_index_exprs(' || quote_literal(indname || '_' || i) ||
      ',' || quote_literal(tabname) || ',' || num_cols || ')';
    EXECUTE format(query);
  END LOOP;
END
$func$ LANGUAGE plpgsql;

-- Lock many tables at once
CREATE OR REPLACE FUNCTION lock_tables(num_tables int)
  RETURNS VOID AS
  $func$
  BEGIN
  FOR i IN 1..num_tables LOOP
    EXECUTE format('LOCK t_' || i || ' IN EXCLUSIVE MODE');
  END LOOP;
END
$func$ LANGUAGE plpgsql;
