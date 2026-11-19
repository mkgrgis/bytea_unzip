--SET log_min_messages TO DEBUG1;
--SET client_min_messages TO DEBUG1;
--Testcase 001:
CREATE EXTENSION bytea_unzip;
--Testcase 002:
SELECT bytea_unzip_version() v;
--Testcase 003:
SELECT bytea_unzip_libzip_version() v;

--Testcase 004:
CREATE TABLE test (id int2 not null, b bytea);
--Testcase 005:
\copy test from './sql/test_zip.data';
--Testcase 006:
set timezone to 'UTC';

--Testcase 010:
select id from test;

--Testcase 011:
select bytea_is_zip(b), id from test;
--Testcase 012: ERR
select b.*, id
  from test, bytea_unzip_list_files(b) b;

--Testcase 013:
select b.*, id
  from test, bytea_unzip_list_files(b) b
 where id = 2;

--Testcase 014:
select convert_from(bytea_unzip(b, 'mimetype'), 'UTF8') t,
       id
  from test
 where id = 2;
--Testcase 012:
select convert_from(bytea_unzip(b, 'META-INF/manifest.xml'), 'UTF8')::xml,
       id
  from test
 where id = 2;

--Testcase 013:
create view spreadsheets as
with 
xml_content as (
select id,
       'content.xml' filename,
       convert_from(bytea_unzip(b, 'content.xml'), 'UTF8')::xml xml,
       ARRAY[ARRAY['office','urn:oasis:names:tc:opendocument:xmlns:office:1.0'],
             ARRAY['table', 'urn:oasis:names:tc:opendocument:xmlns:table:1.0']
            ] odf_ns
  from test
 where id = 2  
),
table_xml as (
select filename, id,
       unnest(xpath('//office:body/office:spreadsheet/table:table', xml, odf_ns)) table_xml,
       unnest(xpath('//office:body/office:spreadsheet/table:table/@table:name', xml, odf_ns))::text table_name
       from xml_content
)
select id,
       filename,
       row_number() over (partition by filename) spreadsheet_no,
       table_name,
       table_xml
from table_xml;
--Testcase 014:
select id,
       filename,
       spreadsheet_no,
       table_name,
       octet_length(table_xml::text) l
  from spreadsheets;

--Testcase 015:
select s.spreadsheet_no,
       s.table_name,
       decompose_ods_table_node(s.table_xml) cell_arr
  from spreadsheets s
 where spreadsheet_no = 1;

--Testcase 040:
DROP EXTENSION bytea_unzip CASCADE;
