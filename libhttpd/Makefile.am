#
# $Id$
#

lib_LTLIBRARIES = libhttpd.la

libhttpd_la_SOURCES = protocol.c \
	api.c \
	version.c \
	ip_acl.c \
	debug.c

noinst_HEADERS = httpd_priv.h

pkginclude_HEADERS = httpd.h debug.h

EXTRA_DIST = README

AM_CPPFLAGS = \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-g
