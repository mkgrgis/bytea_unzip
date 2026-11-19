/*-------------------------------------------------------------------------
 *
 * ZIP data reader (extractor) for PotgreSQL bytea data
 *
 * (c) 2026, mkgrgis
 *
 * IDENTIFICATION
 *		bytea_unzip.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "bytea_unzip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>

#include "fmgr.h"
#include "storage/ipc.h"
#include "utils/builtins.h"

#if PG_VERSION_NUM >= 160000
	#include "varatt.h"
#else
	#include "lib/stringinfo.h"
#endif

#include "funcapi.h"
#include "utils/timestamp.h"
#include "datatype/timestamp.h"
#include "utils/array.h"
#include "utils/xml.h"
#include "catalog/pg_type.h"

#include <libxml/parser.h>
#include <libxml/tree.h>


Datum bytea_unzip_version(PG_FUNCTION_ARGS);
Datum bytea_unzip_libzip_version(PG_FUNCTION_ARGS);
Datum bytea_is_zip(PG_FUNCTION_ARGS);
Datum bytea_has_zippedfile(PG_FUNCTION_ARGS);
Datum bytea_unzip(PG_FUNCTION_ARGS);
Datum bytea_unzip_list_files(PG_FUNCTION_ARGS);
Datum decompose_ods_table_node(PG_FUNCTION_ARGS);

static void bytea_unzip_exit(int code, Datum arg);
zip_source_t* get_zip_source (const char *buf, size_t len);
zip_file_t* get_zipped_file (const char *buf, size_t len, char* fileaddr, zip_t* za);
int unzip(const char *buf, size_t len, char* fileaddr, char **out, size_t *out_len);


extern PGDLLEXPORT void _PG_init(void);
static void bytea_unzip_exit(int code, Datum arg);

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(bytea_unzip_version);
PG_FUNCTION_INFO_V1(bytea_unzip_libzip_version);
PG_FUNCTION_INFO_V1(bytea_is_zip);
PG_FUNCTION_INFO_V1(bytea_has_zippedfile);
PG_FUNCTION_INFO_V1(bytea_unzip_list_files);
PG_FUNCTION_INFO_V1(bytea_unzip);
PG_FUNCTION_INFO_V1(decompose_ods_table_node);

/*
 * Library load-time initialization, sets on_proc_exit() callback for
 * backend shutdown.
 */
void
_PG_init(void)
{
	on_proc_exit(&bytea_unzip_exit, PointerGetDatum(NULL));
}

/*
 * bytea_unzip_exit: Exit callback function.
 */
static void
bytea_unzip_exit(int code, Datum arg)
{
	// No opetations
}

Datum
bytea_unzip_version(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(CODE_VERSION);
}

Datum
bytea_unzip_libzip_version(PG_FUNCTION_ARGS)
{
	int32_t res = (LIBZIP_VERSION_MAJOR * 10000) +
			   (LIBZIP_VERSION_MINOR * 100) +
			   (LIBZIP_VERSION_MICRO);
	PG_RETURN_INT32((int32_t)res);
/*	Dynamic version
	const char *ver_str = zip_libzip_version();
	int major = 0, minor = 0, micro = 0, res = 0;

	if (ver_str != NULL) {
		sscanf(ver_str, "%d.%d.%d", &major, &minor, &micro);
	}

	// Option A: Decimal packing (e.g., 1.12.1 -> 11201)
	res = (int32_t)(major * 10000 + minor * 100 + micro);
	PG_RETURN_INT32(res);
*/
}

Datum
bytea_is_zip(PG_FUNCTION_ARGS)
{
bytea *data;
	char *raw_bytes;
	size_t len;
	zip_error_t zerr;
	zip_source_t *src;
	zip_t *za;
	bool is_valid = false;

	if (PG_ARGISNULL(0))
		PG_RETURN_BOOL(false);

	/* Detoast the input bytea (use SLICE or standard copy-free macro if large) */
	data = PG_GETARG_BYTEA_PP(0);
	raw_bytes = VARDATA_ANY(data);
	len = VARSIZE_ANY_EXHDR(data);

	/* Quick rejection: minimum valid empty ZIP archive is 22 bytes */
	if (len < 22)
	{
		PG_FREE_IF_COPY(data, 0);
		PG_RETURN_BOOL(false);
	}

	zip_error_init(&zerr);

	/* Create a libzip source from memory (freep = 0 because PostgreSQL owns memory) */
	src = zip_source_buffer_create(raw_bytes, (zip_uint64_t) len, 0, &zerr);
	if (src != NULL)
	{
		/* Open the archive in read-only mode */
		za = zip_open_from_source(src, ZIP_RDONLY, &zerr);
		if (za != NULL)
		{
			is_valid = true;
			/* zip_close frees 'src' automatically on success */
			zip_close(za);
		}
		else
		{
			/* Must manually free 'src' if zip_open_from_source fails */
			zip_source_free(src);
		}
	}

	zip_error_fini(&zerr);
	PG_FREE_IF_COPY(data, 0);

	PG_RETURN_BOOL(is_valid);
}

Datum
bytea_unzip(PG_FUNCTION_ARGS)
{
	bytea *zip_data;
	text *filename_text;
	char *filename;
	
	zip_error_t ziperror;
	zip_source_t *src;
	zip_t *za;
	zip_stat_t st;
	zip_file_t *zf;
	
	bytea *result;
	zip_int64_t bytes_read;

	/* Handle NULL inputs safely (though STRICT in SQL also handles this) */
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();

	/* Detoast input arguments to ensure contiguous memory */
	zip_data = PG_GETARG_BYTEA_PP(0);
	filename_text = PG_GETARG_TEXT_PP(1);
	filename = text_to_cstring(filename_text);

	zip_error_init(&ziperror);

	/* Create a libzip source directly from the bytea payload in memory */
	src = zip_source_buffer_create(
		VARDATA_ANY(zip_data),
		VARSIZE_ANY_EXHDR(zip_data),
		0, /* freep = 0: PostgreSQL palloc/detoast manages this memory */
		&ziperror
	);

	if (src == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("failed to create zip source buffer: %s", zip_error_strerror(&ziperror))));
	}

	/* Open the zip archive from the memory source */
	za = zip_open_from_source(src, ZIP_RDONLY, &ziperror);
	if (za == NULL)
	{
		zip_source_free(src);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("failed to open bytea-zip archive: %s", zip_error_strerror(&ziperror))));
	}

	/* Locate the file inside the archive and get its uncompressed size */
	zip_stat_init(&st);
	if (zip_stat(za, filename, 0, &st) != 0)
	{
		zip_close(za);
		zip_error_fini(&ziperror);
		pfree(filename);
		/* Return NULL if the requested file path does not exist in the archive */
		PG_RETURN_NULL();
	}

	/* Ensure the uncompressed size is valid and fits within PostgreSQL's varlena limit, usually 1Gb */
	if (!(st.valid & ZIP_STAT_SIZE))
	{
		zip_close(za);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("could not determine uncompressed size for \"%s\"", filename)));
	}

	if (st.size > MaxAllocSize - VARHDRSZ)
	{
		zip_close(za);
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("uncompressed file \"%s\" exceeds PostgreSQL bytea limit", filename)));
	}

	/* Open the specific file inside the archive */
	zf = zip_fopen(za, filename, 0);
	if (zf == NULL)
	{
		const char *err_str = zip_strerror(za);
		zip_close(za);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("failed to open file \"%s\" in bytea-archive: %s", filename, err_str)));
	}

	/* Allocate PostgreSQL bytea for the uncompressed output */
	result = (bytea *) palloc(st.size + VARHDRSZ);
	SET_VARSIZE(result, st.size + VARHDRSZ);

	/* Read the uncompressed bytes into the result buffer */
	bytes_read = zip_fread(zf, VARDATA(result), st.size);
	if (bytes_read < 0 || (zip_uint64_t) bytes_read != st.size)
	{
		zip_fclose(zf);
		zip_close(za);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("failed to read complete file \"%s\" from bytea-archive", filename)));
	}

	/* Clean up libzip resources */
	zip_fclose(zf);
	zip_close(za); /* Also frees 'src' automatically upon success */
	zip_error_fini(&ziperror);
	pfree(filename);

	PG_RETURN_BYTEA_P(result);
}

Datum
bytea_unzip_list_files(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	bytea		 *zip_data;
	char		  *raw_data;
	size_t		 raw_len;
	zip_error_t	zerror;
	zip_source_t  *src;
	zip_t		 *za;
	zip_int64_t	num_entries, i;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	/* Initialize the Materialized Set-Returning Function */
	InitMaterializedSRF(fcinfo, 0);

	/* Extract raw bytes and length from the bytea argument */
	zip_data = PG_GETARG_BYTEA_PP(0);
	raw_data = VARDATA_ANY(zip_data);
	raw_len  = VARSIZE_ANY_EXHDR(zip_data);

	zip_error_init(&zerror);

	/* Create an in-memory libzip source from the bytea buffer (0 = do not free buffer) */
	src = zip_source_buffer_create(raw_data, raw_len, 0, &zerror);
	if (src == NULL)
	{
		char *err_msg = pstrdup(zip_error_strerror(&zerror));
		zip_error_fini(&zerror);
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("failed to create zip buffer source: %s", err_msg)));
	}

	/* Open the zip archive from the memory source */
	za = zip_open_from_source(src, ZIP_RDONLY, &zerror);
	if (za == NULL)
	{
		char *err_msg = pstrdup(zip_error_strerror(&zerror));
		zip_source_free(src);
		zip_error_fini(&zerror);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("failed to open zip archive from bytea: %s", err_msg)));
	}

	num_entries = zip_get_num_entries(za, 0);

	for (i = 0; i < num_entries; i++)
	{
		zip_stat_t sb;
		Datum	  values[4];
		bool	   nulls[4] = {false, false, false, false};

		zip_stat_init(&sb);

		if (zip_stat_index(za, i, 0, &sb) == 0)
		{
			/* 1. File name */
			if (sb.valid & ZIP_STAT_NAME)
				values[0] = CStringGetTextDatum(sb.name);
			else
				nulls[0] = true;

			/* 2. Uncompressed size */
			if (sb.valid & ZIP_STAT_SIZE)
				values[1] = Int64GetDatum((int64) sb.size);
			else
				nulls[1] = true;

			/* 3. Compressed size */
			if (sb.valid & ZIP_STAT_COMP_SIZE)
				values[2] = Int64GetDatum((int64) sb.comp_size);
			else
				nulls[2] = true;

			/* 4. Modification time (converted from Unix time_t to Postgres timestamptz) */
			if (sb.valid & ZIP_STAT_MTIME)
				values[3] = TimestampTzGetDatum(time_t_to_timestamptz(sb.mtime));
			else
				nulls[3] = true;

			tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
		}
	}

	/* Close archive (automatically frees 'src' as well) */
	zip_close(za);
	zip_error_fini(&zerror);

	return (Datum) 0;
}

/* State maintained across SRF calls */
typedef struct
{
	xmlDocPtr   doc;
	xmlNodePtr  cur_row;
} OdsParseState;

/*
 * Helper: Extract concatenated text from all <text:p> children of a <table:table-cell>.
 * Returns a palloc'd C string, or NULL if the cell has no text.
 */
static char *
extract_cell_text(xmlNodePtr cell_node)
{
	StringInfoData buf;
	bool has_text = false;
	xmlNodePtr child;

	initStringInfo(&buf);

	for (child = cell_node->children; child != NULL; child = child->next)
	{
		if (child->type == XML_ELEMENT_NODE &&
			xmlStrcmp(child->name, (const xmlChar *) "p") == 0)
		{
			xmlChar *content = xmlNodeGetContent(child);
			if (content != NULL)
			{
				if (has_text)
					appendStringInfoChar(&buf, '\n');

				appendStringInfoString(&buf, (const char *) content);
				has_text = true;
				xmlFree(content);
			}
		}
	}

	if (!has_text)
	{
		pfree(buf.data);
		return NULL;
	}

	return buf.data;
}

/*
 * Helper: Advance to the next <table:table-row> element sibling.
 */
static xmlNodePtr
next_table_row(xmlNodePtr node)
{
	while (node != NULL)
	{
		if (node->type == XML_ELEMENT_NODE &&
			xmlStrcmp(node->name, (const xmlChar *) "table-row") == 0)
		{
			return node;
		}
		node = node->next;
	}
	return NULL;
}

Datum
decompose_ods_table_node(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	OdsParseState   *state;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		xmltype	*xml_input = PG_GETARG_XML_P(0);
		text	   *xml_text;
		char	   *xml_str;
		xmlNodePtr  root;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		state = (OdsParseState *) palloc(sizeof(OdsParseState));

		/* Convert PostgreSQL xml datum to C string */
		xml_text = xmltotext_with_options(xml_input, XMLOPTION_CONTENT, false);
		xml_str = text_to_cstring(xml_text);

		/* Parse XML document */
		state->doc = xmlReadMemory(xml_str, strlen(xml_str), "ods_table.xml", NULL,
								   XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
		if (state->doc == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_XML_DOCUMENT),
					 errmsg("could not parse ODS table XML")));

		root = xmlDocGetRootElement(state->doc);
		if (root == NULL || xmlStrcmp(root->name, (const xmlChar *) "table") != 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_XML_CONTENT),
					 errmsg("root XML element must be <table:table>")));

		state->cur_row = next_table_row(root->children);
		funcctx->user_fctx = state;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	state = (OdsParseState *) funcctx->user_fctx;

	if (state->cur_row != NULL)
	{
		Datum result_array;
		xmlNodePtr  row_node = state->cur_row;
		xmlNodePtr  cell_node;
		ArrayBuildState *astate = NULL;
		int		 pending_trailing_nulls = 0;

		/* Advance pointer for the next SRF call */
		state->cur_row = next_table_row(row_node->next);

		for (cell_node = row_node->children; cell_node != NULL; cell_node = cell_node->next)
		{
			xmlChar *repeat_attr;
			int	  repeat_count = 1;
			char	*cell_val;

			if (cell_node->type != XML_ELEMENT_NODE)
				continue;

			/* Handle both regular cells and covered (merged) cells */
			if (xmlStrcmp(cell_node->name, (const xmlChar *) "table-cell") != 0 &&
				xmlStrcmp(cell_node->name, (const xmlChar *) "covered-table-cell") != 0)
				continue;

			/* Check table:number-columns-repeated */
			repeat_attr = xmlGetProp(cell_node, (const xmlChar *) "number-columns-repeated");
			if (repeat_attr != NULL)
			{
				repeat_count = atoi((const char *) repeat_attr);
				if (repeat_count < 1)
					repeat_count = 1;
				xmlFree(repeat_attr);
			}

			cell_val = extract_cell_text(cell_node);

			if (cell_val == NULL)
			{
				/*
				 * Defer adding NULLs. ODS files often pad the end of a row with
				 * thousands of repeated empty cells (e.g. number-columns-repeated="1024").
				 * By deferring, we strip trailing empty cells while preserving empty cells
				 * that appear before a populated cell.
				 */
				pending_trailing_nulls += repeat_count;
			}
			else
			{
				Datum text_datum = CStringGetTextDatum(cell_val);

				/* Flush any empty cells that occurred before this non-empty cell */
				while (pending_trailing_nulls > 0)
				{
					astate = accumArrayResult(astate, (Datum) 0, true, TEXTOID, CurrentMemoryContext);
					pending_trailing_nulls--;
				}

				for (int i = 0; i < repeat_count; i++)
				{
					astate = accumArrayResult(astate, text_datum, false, TEXTOID, CurrentMemoryContext);
				}

				pfree(cell_val);
			}
		}

		if (astate == NULL)
		{
			/* Empty row: return empty text[] '{}' */
			result_array = PointerGetDatum(construct_empty_array(TEXTOID));
		}
		else
		{
			result_array = makeArrayResult(astate, CurrentMemoryContext);
		}

		SRF_RETURN_NEXT(funcctx, result_array);
	}
	else
	{
		/* Cleanup libxml2 document before finishing */
		if (state->doc != NULL)
		{
			xmlFreeDoc(state->doc);
			state->doc = NULL;
		}
		SRF_RETURN_DONE(funcctx);
	}
}
