/* pg_sasl_prepare/pg_sasl_prepare--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_sasl_prepare" to load this file. \quit

-- This is a pg_sasl_prepare
CREATE FUNCTION pg_sasl_prepare(_int4)
RETURNS _int4
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- Conversion functions
CREATE FUNCTION utf8_to_array(text)
RETURNS _int4
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

CREATE FUNCTION array_to_utf8(_int4)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

-- Conversion table fetch
CREATE OR REPLACE FUNCTION utf8_conv_table(
    OUT code int,
    OUT class smallint,
    OUT decomposition _int4)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;
