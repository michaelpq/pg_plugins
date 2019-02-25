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
