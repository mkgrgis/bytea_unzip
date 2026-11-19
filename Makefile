######################################################################-------------------------------------------------------------------------
#
# ZIP content decompressor (reader) from bytea data for PostgreSQL
#
# Portions Copyright (c) 2026, mkgrgis
#
# IDENTIFICATION
# 		Makefile
#
##########################################################################

MODULE_big = bytea_unzip
OBJS = bytea_unzip.o 

EXTENSION = bytea_unzip
DATA = bytea_unzip--1.0.sql

ifndef REGRESS
REGRESS = bytea_unzip
endif

REGRESS_OPTS = --encoding=utf8

UNAME = uname
OS := $(shell $(UNAME))
ifeq ($(OS), Darwin)
DLSUFFIX = .dylib
else
DLSUFFIX = .so
endif

PG_CPPFLAGS = $(shell xml2-config --cflags)
SHLIB_LINK = $(shell xml2-config --libs) -lzip

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
ifndef MAJORVERSION
MAJORVERSION := $(basename $(VERSION))
endif
ifeq (,$(findstring $(MAJORVERSION), 10 11 12 13 14 15 16 17 18 19))
$(error PostgreSQL 10 11 12 13, 14, 15, 16, 17, 18 or 19 is required to compile this extension)
endif
else
subdir = contrib/bytea_unzip
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

ifdef REGRESS_PREFIX
REGRESS_PREFIX_SUB = $(REGRESS_PREFIX)
else
REGRESS_PREFIX_SUB = $(VERSION)
endif

REGRESS := $(addprefix $(REGRESS_PREFIX_SUB)/,$(REGRESS))
$(shell mkdir -p results/$(REGRESS_PREFIX_SUB))
