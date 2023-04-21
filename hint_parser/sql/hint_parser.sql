CREATE EXTENSION hint_parser;
-- Hints with correct format
SELECT hint_parser('i(i)');
SELECT hint_parser('hint1(content1)hint2(content2)');
SELECT hint_parser('hint1( content1 ) hint2  ( content2 )');
SELECT hint_parser('  hint1 ( content1 ) h2 (c2) h3 (c3 )');
-- Broken hints
SELECT hint_parser('');
SELECT hint_parser('I');
SELECT hint_parser('I (AA) I2');
SELECT hint_parser('I( II ) I2( I099 ) I3 (UUKA');
