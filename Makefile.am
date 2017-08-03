
AM_CPPFLAGS = -std=c++11 $(PTHREAD_CFLAGS) \
	-I$(top_srcdir)/vendor/liquibook/src

sbin_PROGRAMS = obsrv obdb

noinst_LIBRARIES = libobcommon.a

libobcommon_a_SOURCES = \
	Util.h Util.cc

obsrv_SOURCES = \
	srvapi.h srvapi.cc \
	srv.h obsrv.cc \
	Market.h Market.cc \
	OrderFwd.h Order.h Order.cc \
	HttpUtil.h HttpUtil.cc

obsrv_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS)
obsrv_LDADD = \
	libobcommon.a \
	$(PTHREAD_LIBS)		\
	-levhtp \
	-lunivalue \
	-levent_core -levent_openssl \
	$(OPENSSL_LIBS) $(ARGP_LIB) $(UUID_LIB) $(ROCKS_LIB)

obdb_SOURCES = obdb.cc
obdb_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS)
obdb_LDADD = \
	libobcommon.a \
	$(PTHREAD_LIBS)		\
	-lunivalue \
	$(OPENSSL_LIBS) $(ARGP_LIB) $(UUID_LIB) $(ROCKS_LIB)

EXTRA_DIST = obsrv-tests.sh test-config-obsrv.json
TESTS = obsrv-tests.sh

