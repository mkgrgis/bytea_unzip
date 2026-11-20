Zip and ODS (ODF) data reader from binary `bytea` data for PostgreSQL
=====================================================================

This is a [PostgreSQL](https://www.postgresql.org/) extension which allows to read compressed ZIP data stored `bytea` value in PostgreSQL. This FDW works with all of PostgreSQL 16+ and confirmed with libzip4 and libxml2.

<img src="https://upload.wikimedia.org/wikipedia/commons/2/29/Postgresql_elephant.svg" align="center" height="100" alt="PostgreSQL"/>	+	<img src="img_logo/logo-zip.svg" align="center" height="100" alt="ZIP"/>+	<img src="img_logo/logo-ods.png" align="center" height="100" alt="ODS"/>

Current CI status.

[![CI Status](https://github.com/mkgrgis/bytea_unzip/actions/workflows/CI.yml/badge.svg)](https://github.com/mkgrgis/bytea_unzip/actions/workflows/CI.yml)

Contents
--------

1. [Features](#features)
2. [Supported platforms](#supported-platforms)
3. [Installation](#installation)
4. [Usage](#usage)
5. [Functions](#functions)
6. [Examples](#examples)
10. [Limitations](#limitations)
11. [Tests](#tests)
12. [Contributing](#contributing)
13. [Useful links](#useful-links)

Features
--------

### Common features

This extension reads decompressed data from a ZIP archive represented as `bytea` value in a PostgreSQL.

Included special function for fast ODS spreadsheet decomposition to per-row
cell `text` array, based on pointed ODF `<table>` XML node.

Also see [Limitations](#limitations)

Supported platforms
-------------------

`bytea_unzip` was developed on Linux and should run on any
reasonably POSIX-compliant system.

Installation
------------

### Package installation

There are no Linux distributives contains internal package with `bytea_unzip`.

Known special runtime dependencies: `libzip4t64` and `libxml2` or similar for your OS
Use `apt-get install libzip4t64 libxml2` to install this.

### Source installation

Prerequisites:
* `gcc`
* `make`
* `postgresql-server-dev`, especially `postgres.h`
* `libzip-dev`, especially `zip.h` headers
* `libxml2-dev`

#### 1. Install C ZIP library and Postgres development libraries

For Debian or Ubuntu:

`apt-get install libzip-dev libxml2-dev -y`

`apt-get install postgresql-server-dev-XX -y`, where XX matches your postgres version, i.e. `apt-get install postgresql-server-dev-19 -y`

#### 2. Build and install bytea_unzip

Before building please add a directory of `pg_config` to PATH or ensure `pg_config` program is accessible from command line only by the name.

```sh
make USE_PGXS=1
make install USE_PGXS=1
```

If you want to build `bytea_unzip` in a source tree of PostgreSQL, use
```sh
make
make install
```

Usage
-----

### Datatypes of result
Spatial functions of this extension returns common OGC `ST_Point` data like `Point(lon lat)`;

Functions
---------

- int **bytea_unzip_version**();

Returns standard "version integer" as `major version * 10000 + minor version * 100 + bugfix`.
```
bytea_odf_unzip_version
--------------------
              10000
```

- int **bytea_unzip_libzip_version**()

Returns version of libzip used by PostgreSQL extension.

- bool **bytea_is_zip** (data bytea);

Returns true if there is zip data container by libzip conditions. Warning: a zip container can be decalred without any useful data. Returns `NULL` for `NULL` bytea.

- bytea **bytea_unzip**(data bytea, filename text);

Returns bytea content of a decompressed file determiner by relative filename inside of the zip archive. If there is no such file leaves `NULL` value.
Throws error if not a zip data.

- table (
      file_name         text,
      uncompressed_size bigint,
      compressed_size   bigint,
      modified_at       timestamptz
  ) **bytea_unzip_list_files**(data bytea);

Returns a table of file names, sizes and timestamps of compressed files in pointed archive data.
Throws error if not a zip data.

- text[] **decompose_ods_table_node**(node xml) 

Returns text[] for cell data of ODS spreadsheet xml node from OpenDocument format content.xml.

Examples
--------

### Install the extension:

Once for a database you need, as PostgreSQL superuser.

```sql
	CREATE EXTENSION bytea_unzip;

-- only for testing or for example playground
	CREATE EXTENSION http;
```

### Decompress and use ZIP file content from internet:

```sql
-- ODF ods file example
-- Playground data source
create materialized view test as
with url as (
select 'https://moscowparks.narod.ru/docs/explication.ods' url
)
select text_to_bytea(h.content) b,
       url.url
  from url,
       http_get(url.url) h;

-- Check if this is zip archive
select bytea_is_zip(b), url from test;

-- List internal files a content of a zip data
select b.*, url
  from test, bytea_unzip_list_files(b) b;

-- Use some unzipped data
select convert_from(bytea_unzip(b, 'mimetype'), 'UTF8') t,
       url
  from test;

-- Transformation of some unzipped data
select convert_from(bytea_unzip(b, 'META-INF/manifest.xml'), 'UTF8')::xml,
       url
  from test;

create view  as
with xml_content as (
select url,
       'content.xml' filename,
       convert_from(bytea_unzip(b, 'content.xml'), 'UTF8')::xml xml,
       ARRAY[ARRAY['office','urn:oasis:names:tc:opendocument:xmlns:office:1.0'],
             ARRAY['table', 'urn:oasis:names:tc:opendocument:xmlns:table:1.0']
            ] odf_ns
from  test
),
table_xml as (
select filename, url,
       unnest(xpath('//office:body/office:spreadsheet/table:table', xml, odf_ns)) table_xml,
       unnest(xpath('//office:body/office:spreadsheet/table:table/@table:name', xml, odf_ns))::text table_name
       from xml_content
)
select url,
       filename,
       row_number() over (partition by filename) "spreadsheet No",
       table_name,
       table_xml
from table_xml;

select s."spreadsheet No",
       s.table_name
  from "ODF XML sheets" s;

select s."spreadsheet No",
       s.table_name,
       decompose_ods_table_node(s.table_xml::xml) cell_arr
  from "ODF XML sheets" s
 where "spreadsheet No" = 1;
```

Limitations
-----------

No special limitations are known.

Tests
-----
Test directory have structure as following:

```
+---sql
|   +---13.15
|   |       filename1.sql
|   |       filename2.sql
|   |
|   +---14.12
|   |       filename1.sql
|   |       filename2.sql
|   |
.................
|   \---17.0
|          filename1.sql
|          filename2.sql
|
\---expected
|   +---13.15
|   |       filename1.out
|   |       filename2.out
|   |
|   +---14.12
|   |       filename1.out
|   |       filename2.out
|   |
.................
|   \---17.0
            filename1.out
            filename2.out
```
The test cases for each version are based on the test of corresponding version of PostgreSQL.
You can execute test by test.sh directly.
The version of PostgreSQL is detected automatically by $(VERSION) variable in Makefile.
The corresponding sql and expected directory will be used to compare the result. For example, for Postgres 15.0, you can execute "test.sh" directly, and the sql/15.0 and expected/15.0 will be used to compare automatically.

Contributing
------------

Opening issues and pull requests on GitHub are welcome.
For pull request, please make sure these items below for testing:
- Create test cases (if needed) for the latest version of PostgreSQL supported by `bytea_unzip`. All error testcases should have a comment about test purpose.
- Execute test cases and update expectations for the latest version of PostgreSQL
- Test creation and execution for other PostgreSQL versions are welcome but not required.

Preferred code style see in PostgreSQL source codes. For example

```C
type
funct_name (type arg ...)
{
	t1 var1 = value1;
	t2 var2 = value2;

	for (;;)
	{
	}
	if ()
	{
	}
}
```
Useful links
------------

### Source

 - https://pgxn.org/dist/bytea_unzip/

License
-------
* Copyright © 2026, mkgrgis

PostgreSQL license
Permission to use, copy, modify, and distribute this software and its documentation for any purpose, without fee, and without a written agreement is hereby granted, provided that the above copyright notice and this paragraph and the following two paragraphs appear in all copies.

See the [`License`](License) file for full details.
