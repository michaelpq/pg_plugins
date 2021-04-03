CREATE EXTENSION hmac_funcs;
SELECT hmac_md5('The quick brown fox jumps over the lazy dog', 'key');
SELECT hmac_sha1('The quick brown fox jumps over the lazy dog', 'key');
SELECT hmac_sha224('The quick brown fox jumps over the lazy dog', 'key');
SELECT hmac_sha256('The quick brown fox jumps over the lazy dog', 'key');
SELECT hmac_sha384('The quick brown fox jumps over the lazy dog', 'key');
SELECT hmac_sha512('The quick brown fox jumps over the lazy dog', 'key');
