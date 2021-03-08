-- Create a custom aggregate
CREATE OR REPLACE FUNCTION sum_custom(data1 int, data2 int)
RETURNS int AS $$
BEGIN
  RAISE NOTICE 'function called';
  RETURN data1 + data2;
END;
$$ LANGUAGE plpgsql;

CREATE AGGREGATE sum_custom_agg (int)
(
  sfunc = sum_custom,
  stype = int,
  initcond = 0
);
CREATE TABLE sum_data AS SELECT generate_series(1, 10) AS id;
SELECT sum_custom(1, 3);
SELECT sum_custom_agg (id) FROM sum_data;
