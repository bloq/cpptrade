
AM_CPPFLAGS = -std=c++11 $(PTHREAD_CFLAGS)

sbin_PROGRAMS = obsrv

obsrv_SOURCES = \
	srvapi.h srvapi.cc \
	srv.h obsrv.cc \
	Market.h Market.cc \
	OrderFwd.h Order.h Order.cc \
	HttpUtil.h HttpUtil.cc \
	Util.h Util.cc

obsrv_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS)
obsrv_LDADD = \
	$(PTHREAD_LIBS)		\
	-levhtp \
	-lunivalue \
	-levent_core -levent_openssl \
	$(OPENSSL_LIBS) $(ARGP_LIB) $(UUID_LIB)

EXTRA_DIST = obsrv-tests.sh
TESTS = obsrv-tests.sh

