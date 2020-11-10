## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = gnits

SUBDIRS = doc lib po . test

EXTRA_DIST = config.rpath mkinstalldirs autogen.sh BUGS
EXTRA_PROGRAMS = secret-query secret-ask
bin_PROGRAMS = agpg apgp q-agent q-client @GTK_STUFF@

localedir = $(datadir)/locale
AM_CPPFLAGS = -I$(top_srcdir)/lib -Iintl -DLOCALEDIR=\"$(localedir)\" \
	$(GLIB_CFLAGS) $(GTK_CFLAGS)

LDADD = lib/libutil.a @LIBINTL@ $(LIBCAP)

agpg_SOURCES = agpg.c agentlib.c util.c secmem.c memory.h

apgp_SOURCES = apgp.c agentlib.c util.c secmem.c memory.h

secret_ask_LDADD = lib/libutil.a @LIBINTL@ $(GTK_LIBS)
secret_ask_SOURCES = secret-ask.c i18n.h

secret_query_LDADD = lib/libutil.a @LIBINTL@ $(GTK_LIBS) $(LIBCAP)
secret_query_SOURCES = secret-query.c i18n.h gtksecentry.c gtksecentry.h \
	secmem.c memory.h util.c util.h

q_client_SOURCES = client.c agent.h agentlib.c agentlib.h util.c util.h \
	i18n.h secmem.c memory.h

q_agent_LDADD = lib/libutil.a @LIBINTL@ $(GLIB_LIBS) $(LIBCAP)
q_agent_SOURCES = agent.c agent.h util.c util.h secmem.c i18n.h memory.h

lib/libutil.a:
	cd lib && $(MAKE) $(AM_MAKEFLAGS) libutil.a

agent.o: agent.c
	$(COMPILE) $(GLIB_CFLAGS) -c $<

install-exec-local:
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  echo "  $(SETCAP) cap_ipc_lock+p $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
	  $(SETCAP) cap_ipc_lock+p $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'` || true; \
	done

distclean-local:
	rm -f config.cache config.log build-stamp

signed-dist: $(distdir).tar.gz.sig

%.sig: %
	agpg --detach-sign $<

ACLOCAL_AMFLAGS = -I m4
