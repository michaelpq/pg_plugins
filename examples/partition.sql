-- partitions

-- Template of multi-layer partitions with data, with range
CREATE TABLE parent_tab (id int) PARTITION BY RANGE (id);
CREATE INDEX parent_index ON parent_tab (id);
CREATE TABLE child_0_10 PARTITION OF parent_tab
     FOR VALUES FROM (0) TO (10);
CREATE TABLE child_10_20 PARTITION OF parent_tab
     FOR VALUES FROM (10) TO (20);
CREATE TABLE child_20_30 PARTITION OF parent_tab
     FOR VALUES FROM (20) TO (30);
INSERT INTO parent_tab VALUES (generate_series(0,29));
CREATE TABLE child_30_40 PARTITION OF parent_tab
     FOR VALUES FROM (30) TO (40)
     PARTITION BY RANGE(id);
CREATE TABLE child_30_35 PARTITION OF child_30_40
     FOR VALUES FROM (30) TO (35);
CREATE TABLE child_35_40 PARTITION OF child_30_40
     FOR VALUES FROM (35) TO (40);
INSERT INTO parent_tab VALUES (generate_series(30,39));

-- Partitions with lists of values
CREATE TABLE parent_list (id int) PARTITION BY LIST (id);
CREATE TABLE child_list PARTITION OF parent_list
     FOR VALUES IN (1, 2);
-- Create one partition with a long list of incremented values, to emulate
-- large tuple sizes of pg_class with the partition bound definition.
CREATE OR REPLACE FUNCTION create_long_list(tabname text, tabparent text,
  num_vals int)
RETURNS VOID AS
$func$
DECLARE
  query text;
BEGIN
  query := 'CREATE TABLE ' || tabname ||
           ' PARTITION OF ' || tabparent || ' FOR VALUES IN (';
  FOR i IN 1..num_vals LOOP
    query := query || i;
    IF i != num_vals THEN
      query := query || ', ';
    END IF;
  END LOOP;
  query := query || ')';
  EXECUTE format(query);
END
$func$ LANGUAGE plpgsql;
