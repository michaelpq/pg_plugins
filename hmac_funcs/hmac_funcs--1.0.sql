/* hmac_funcs/hmac_funcs--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION hmac_funcs" to load this file. \quit

-- HMAC functions
CREATE OR REPLACE FUNCTION hmac_md5(bytea, bytea)
  RETURNS bytea
  LANGUAGE C
  AS 'MODULE_PATHNAME';
CREATE OR REPLACE FUNCTION hmac_sha1(bytea, bytea)
  RETURNS bytea
  LANGUAGE C
  AS 'MODULE_PATHNAME';
CREATE OR REPLACE FUNCTION hmac_sha224(bytea, bytea)
  RETURNS bytea
  LANGUAGE C
  AS 'MODULE_PATHNAME';
CREATE OR REPLACE FUNCTION hmac_sha256(bytea, bytea)
  RETURNS bytea
  LANGUAGE C
  AS 'MODULE_PATHNAME';
CREATE OR REPLACE FUNCTION hmac_sha384(bytea, bytea)
  RETURNS bytea
  LANGUAGE C
  AS 'MODULE_PATHNAME';
CREATE OR REPLACE FUNCTION hmac_sha512(bytea, bytea)
  RETURNS bytea
  LANGUAGE C
  AS 'MODULE_PATHNAME';
