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
INSERT INTO parent_list VALUES (1), (2);
