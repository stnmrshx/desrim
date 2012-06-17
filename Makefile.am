#
# Desrim 1.0 Library Makefile
#

## process this file with automake to produce Makefile.in

EXTRA_DIST = README LICENSE

AUTOMAKE_OPTIONS = 

INCLUDES = $(LIBINCLUDES)

lib_LTLIBRARIES = libdesrim.la libdsmalloc.la 

libdesrim_la_SOURCES = desrim.c desrim_plat.c

libdsmalloc_la_SOURCES = desrim_malloc.c

include_HEADERS = desrim.h desrim_internal.h desrim_plat.h desrim_queue.h

