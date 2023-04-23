CREATE EXTENSION hint_parser;
-- Hints with correct format
SELECT * FROM hint_parser('i(i)');
SELECT * FROM hint_parser('hint1(content1)hint2(content2)');
SELECT * FROM hint_parser('hint1( content1 ) hint2  ( content2 )');
SELECT * FROM hint_parser('  hint1 ( content1 ) h2 (c2) h3 (c3 )');
-- Valid, with newlines
SELECT * FROM hint_parser('
h1
  (
  c1
  )
h2(c2) h3(c3)
  h4(c4)
');
-- Broken hints
SELECT hint_parser('');
SELECT hint_parser('I');
SELECT hint_parser('I (AA) I2');
SELECT hint_parser('I( II ) I2( I099 ) I3 (UUKA');

DROP EXTENSION hint_parser;
