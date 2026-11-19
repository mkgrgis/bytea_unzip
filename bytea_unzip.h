/*-------------------------------------------------------------------------
 *
 * EXIF data extractor for PotgreSQL bytea data
 *
 * Copyright (c) 2026, mkgrgis
 *
 * IDENTIFICATION
 *        bytea_unzip.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef BYTEA_UNZIP_H
#define BYTEA_UNZIP_H

#define CODE_VERSION 100000


#include <math.h>
#include <string.h>

#if PG_VERSION_NUM < 120000
/* NullableDatum is introduced from PG12, we define it here in case of PG11 or earlier. */
typedef struct NullableDatum
{
#define FIELDNO_NULLABLE_DATUM_DATUM 0
    Datum        value;
#define FIELDNO_NULLABLE_DATUM_ISNULL 1
    bool        isnull;
    /* due to alignment padding this could be used for flags for free */
} NullableDatum;
#endif

#endif	/* BYTEA_UNZIP_H */
