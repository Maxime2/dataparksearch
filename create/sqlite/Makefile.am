EXTRA_DIST= crc-multi.txt create.txt cross-crc.txt drop.cross-crc.txt \
	drop.crc-multi.txt drop.multi.txt drop.txt multi.txt \
	crc.txt drop.crc.txt

all: create drop

create: create.single.sql create.multi.sql create.crc-multi.sql create.cache.sql
drop: drop.single.sql drop.multi.sql drop.crc-multi.sql drop.cache.sql

create.single.sql: create.txt
	cat $(srcdir)/create.txt > $@

create.multi.sql: create.txt multi.txt
	cat  $(srcdir)/create.txt  $(srcdir)/multi.txt > $@

create.crc.sql: create.txt cross-crc.txt crc.txt
	cat $(srcdir)/create.txt $(srcdir)/cross-crc.txt $(srcdir)/crc.txt > $@

create.crc-multi.sql: create.txt crc-multi.txt
	cat  $(srcdir)/create.txt  $(srcdir)/crc-multi.txt > $@

create.cache.sql: create.txt cross-crc.txt
	cat $(srcdir)/create.txt $(srcdir)/cross-crc.txt > $@


drop.single.sql: drop.txt
	cat  $(srcdir)/drop.txt > $@

drop.multi.sql: drop.txt drop.multi.txt
	cat  $(srcdir)/drop.txt  $(srcdir)/drop.multi.txt > $@

drop.crc.sql: drop.txt drop.cross-crc.txt drop.crc.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.cross-crc.txt $(srcdir)/drop.crc.txt > $@

drop.crc-multi.sql: drop.txt drop.crc-multi.txt
	cat  $(srcdir)/drop.txt  $(srcdir)/drop.crc-multi.txt > $@

drop.cache.sql: drop.txt drop.cross-crc.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.cross-crc.txt > $@


CLEANFILES= create.crc-multi.sql \
  create.crc.sql \
  create.multi.sql \
  create.single.sql \
  create.cache.sql \
  drop.crc-multi.sql \
  drop.crc.sql \
  drop.multi.sql \
  drop.single.sql \
  drop.cache.sql

sql_FILES= create.crc-multi.sql \
  create.crc.sql \
  create.multi.sql \
  create.single.sql \
  create.cache.sql \
  drop.crc-multi.sql \
  drop.crc.sql \
  drop.multi.sql \
  drop.single.sql \
  drop.cache.sql

if PORTS
dpstodir=$(pkgdatadir)
else
dpstodir=$(datadir)
endif

install-data-local: $(sql_FILES)
	$(mkinstalldirs) $(DESTDIR)$(dpstodir)/sqlite
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$(dpstodir)/sqlite/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$(dpstodir)/sqlite/$$p; \
	done
	-rmdir $(DESTDIR)$(dpstodir)/sqlite
