-- Install extension for tests
CREATE EXTENSION wal_utils;

CREATE TABLE history_data (data text);

-- Note that COPY truncates newlines.
-- Load history file that succeeds
INSERT INTO history_data VALUES (
'# Ignore this comment
1       0/09D4F390      no recovery target specified
2       0/117BEB70      no recovery target specified
3       0/187BEB38      no recovery target specified
# Other this comment and empty line

4       0/188BEB38      no recovery target specified
5       0/189BEB38      no recovery target specified
7       0/249BEB38      no recovery target specified
');
SELECT parse_wal_history(data) FROM history_data;
DROP TABLE history_data;
