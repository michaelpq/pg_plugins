/* scram_utils/scram_utils--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION scram_utils" to load this file. \quit

-- This is a scram_utils
CREATE FUNCTION scram_utils_verifier(
  IN username text,
  IN password text,
  IN iterations int,
  IN saltlen int)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
