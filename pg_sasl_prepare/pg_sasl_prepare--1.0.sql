/* pg_sasl_prepare/pg_sasl_prepare--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_sasl_prepare" to load this file. \quit

-- This is a pg_sasl_prepare
CREATE FUNCTION pg_sasl_prepare(text)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
