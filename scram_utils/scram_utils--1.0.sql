/* scram_utils/scram_utils--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION scram_utils" to load this file. \quit

--
-- scram_utils_verifier
--
-- Create SCRAM verifier with a text password.
--
CREATE FUNCTION scram_utils_verifier(
  IN username text,
  IN password text,
  IN iterations int,
  IN saltlen int)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

--
-- scram_utils_verifier_bytea
--
-- Create SCRAM verifier with a bytea password.
--
CREATE FUNCTION scram_utils_verifier_bytea(
  IN username text,
  IN password bytea,
  IN iterations int,
  IN saltlen int)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
