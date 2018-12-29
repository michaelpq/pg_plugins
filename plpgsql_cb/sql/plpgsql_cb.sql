--
-- Tests for PL/pgSQL instrumentation callbacks.
--

LOAD 'plpgsql_cb';

-- Functions
-- One level of depth
CREATE OR REPLACE FUNCTION func_1()
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  x := 2;
  RETURN;
END;
$$;

-- Two levels of depth
CREATE OR REPLACE FUNCTION func_2()
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  BEGIN
    x := 3;
    RETURN;
  END;
END
$$;

-- Check footprint
SELECT func_1();
SELECT func_2();

DROP FUNCTION func_1();
DROP FUNCTION func_2();


-- Event triggers
-- One level of depth
CREATE OR REPLACE FUNCTION event_1()
RETURNS event_trigger
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  x := 2;
  RETURN;
END;
$$;

-- Two levels of depth
CREATE OR REPLACE FUNCTION event_2()
RETURNS event_trigger
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  BEGIN
    x := 3;
    RETURN;
  END;
END
$$;

CREATE EVENT TRIGGER create_table_event ON ddl_command_start
  WHEN TAG IN ('CREATE TABLE') EXECUTE PROCEDURE event_1();
CREATE EVENT TRIGGER drop_table_event ON ddl_command_start
  WHEN TAG IN ('DROP TABLE') EXECUTE PROCEDURE event_2();

-- Check footprint of event trigger execution
CREATE TABLE event_trigger_test (a int);
DROP TABLE event_trigger_test;

DROP EVENT TRIGGER create_table_event;
DROP EVENT TRIGGER drop_table_event;


-- Triggers
-- One level of depth
CREATE FUNCTION trigger_1()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  x := 2;
  RETURN NEW;
END;
$$;

-- Two levels of depth
CREATE FUNCTION trigger_2()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  BEGIN
    x := 3;
    RETURN OLD;
  END;
END
$$;

CREATE TABLE tab_trigger (a int);

CREATE TRIGGER trigger_insert_1
  BEFORE INSERT ON tab_trigger
  FOR EACH ROW EXECUTE PROCEDURE trigger_1();
CREATE TRIGGER trigger_insert_2
  AFTER INSERT ON tab_trigger
  FOR EACH ROW EXECUTE PROCEDURE trigger_2();

-- Check footprint of trigger execution
INSERT INTO tab_trigger VALUES (1);

DROP TABLE tab_trigger;
DROP FUNCTION trigger_1();
DROP FUNCTION trigger_2();
