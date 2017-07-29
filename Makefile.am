
AM_CPPFLAGS = $(PTHREAD_CFLAGS)

sbin_PROGRAMS = obsrv

obsrv_SOURCES = \
	obsrv.cc

obsrv_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS)
obsrv_LDADD = \
	$(PTHREAD_LIBS)		\
	-levhtp \
	-levent_core -levent_extra -levent_openssl \
	$(OPENSSL_LIBS)

