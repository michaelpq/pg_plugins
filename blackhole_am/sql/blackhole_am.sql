CREATE EXTENSION blackhole_am;
CREATE TABLE blackhole_tab (a int) USING blackhole_am;
SELECT * FROM blackhole_tab;
INSERT INTO blackhole_tab VALUES (1);
SELECT * FROM blackhole_tab;
UPDATE blackhole_tab SET a = 0 WHERE a = 1;
SELECT * FROM blackhole_tab;
DELETE FROM blackhole_tab WHERE a = 1;
SELECT * FROM blackhole_tab;
