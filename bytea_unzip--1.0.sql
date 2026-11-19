/* contrib/bytea_unzip/bytea_unzip--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION bytea_unzip" to load this file. \quit

CREATE OR REPLACE FUNCTION bytea_unzip_version()
  RETURNS int
  AS 'MODULE_PATHNAME'
  LANGUAGE C STRICT;

COMMENT ON FUNCTION bytea_unzip_version
IS 'Returns version of bytea zip data processing extension';

CREATE OR REPLACE FUNCTION bytea_unzip_libzip_version()
  RETURNS int
  AS 'MODULE_PATHNAME'
  LANGUAGE C STRICT;

COMMENT ON FUNCTION bytea_unzip_libzip_version
IS 'Returns version of libzip used by PostgreSQL extension';

CREATE OR REPLACE FUNCTION bytea_is_zip(data bytea)
  RETURNS bool
  AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

COMMENT ON FUNCTION bytea_is_zip
IS 'Returns true if there is zip data container by libzip conditions. Warning: the container can be decalred without any useful data.';

CREATE OR REPLACE FUNCTION bytea_unzip_list_files(zip_archive bytea)
  RETURNS TABLE (
      file_name         text,
      uncompressed_size bigint,
      compressed_size   bigint,
      modified_at       timestamptz
  )
  AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

COMMENT ON FUNCTION bytea_unzip_list_files
IS 'Returns a table of file names, sizes and timestamps of compressed files in pointed archive data.';

CREATE OR REPLACE FUNCTION bytea_unzip(data bytea, filename text)
  RETURNS bytea
  AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

COMMENT ON FUNCTION bytea_unzip
IS 'For a selected internal address(name) of compressed file returns decompressed bytea data from pointed archive data stored as bytea.';

CREATE OR REPLACE FUNCTION decompose_ods_table_node(node xml)
  RETURNS SETOF text[]
  AS 'MODULE_PATHNAME'
  LANGUAGE C STRICT IMMUTABLE;

COMMENT ON FUNCTION decompose_ods_table_node
IS 'Returns text[] for ODS spreadsheet xml node from OpenDocument format content.xml.';
