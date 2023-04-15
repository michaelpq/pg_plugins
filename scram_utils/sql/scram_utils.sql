CREATE EXTENSION scram_utils;

CREATE ROLE regress_scram_utils_1;
CREATE ROLE regress_scram_utils_2;

SET scram_iterations = 1000;
SET password_encryption = 'scram-sha-256';
ALTER ROLE regress_scram_utils_1 PASSWORD 'foobar';

-- 3rd argument is the number of iterations, 4th is the salt length.
SELECT scram_utils_verifier('regress_scram_utils_2', 'foobar', 300, 10);

SELECT rolname,
  regexp_replace(rolpassword, '(SCRAM-SHA-256)\$(\d+):([a-zA-Z0-9+/=]+)\$([a-zA-Z0-9+=/]+):([a-zA-Z0-9+/=]+)', '\1$\2:<salt>$<storedkey>:<serverkey>') as rolpassword_masked
  FROM pg_authid WHERE rolname ~ 'regress_scram_utils' ORDER BY rolname;

DROP ROLE regress_scram_utils_1;
DROP ROLE regress_scram_utils_2;
DROP EXTENSION scram_utils;
