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
