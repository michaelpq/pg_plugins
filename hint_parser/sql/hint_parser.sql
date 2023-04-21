CREATE EXTENSION hint_parser;
-- Hint in comments
select hint_parser('/*+h1(h2)*/');
select hint_parser('/*+h1(h2) */');
select hint_parser('/*+h1(h2) */');
select hint_parser('/*+ h1(h2) h3(h4) */');
select hint_parser('/*+ h1(h2) h3(h4) */ /*+ h5(h2) h3(h4) */ /*+ h1(h2) h3(h4) */ ');
