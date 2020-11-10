## Process this file with automake to produce Makefile.in

man_MANS = agpg.1 apgp.1 q-agent.1 secret-ask.1 q-client.1 \
	   secret-query.1

MANUAL = manual.info

EXTRA_DIST = $(man_MANS) agpg.sgml apgp.sgml query-options.sgml \
	     q-agent.sgml secret-ask.sgml q-client.sgml secret-query.sgml \
	     trailer.sgml manual.sgml manual.texi $(MANUAL)

# Where sgmlspl's specs reside. You may have to override this.
# Trailing slash is mandatory.
DBSPECS = /usr/share/sgml/docbook/utils-0.6.12/helpers/

all-local: $(MANUAL)

%.1: %.sgml trailer.sgml
	nsgmls $< | sgmlspl $(DBSPECS)docbook2man-spec.pl

%.texi: %.sgml
	@(set -e; while true; do \
		echo nsgmls $< \| sgmlspl $(DBSPECS)docbook2texi-spec.pl \> $@; \
		nsgmls $< | sgmlspl $(DBSPECS)docbook2texi-spec.pl > $@ 2> $@.log; \
		cat $@.log; \
		grep 'references\? have changed' $@.log >/dev/null || break; \
	done)

## automake's texi support is a bit too much here
%.info: %.texi
	makeinfo $*

q-agent.1 q-client.1:: query-options.sgml

maintainer-clean-local:
	rm -f $(man_MANS) manual.texi manual.info

clean-local:
	rm -f *.refs *.log *.links
