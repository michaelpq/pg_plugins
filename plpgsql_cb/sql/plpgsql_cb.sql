--
-- Tests for PL/pgSQL instrumentation callbacks.
--

LOAD 'plpgsql_cb';

CREATE OR REPLACE FUNCTION public.f1()
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE x int;
BEGIN
  x := 2;
  RETURN;
END;
$$;

CREATE OR REPLACE FUNCTION public.f2()
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

SELECT f1();
SELECT f2();
