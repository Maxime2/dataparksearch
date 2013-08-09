EXTRA_DIST= blob.pgsql crc-multi.txt crc.txt create.txt \
 cross-crc.txt cross.txt drop.crc-multi.txt \
 drop.crc.txt drop.cross-crc.txt drop.cross.txt \
 drop.multi.txt drop.txt grant.txt multi.txt 

all: create drop

create: create.single.sql create.multi.sql create.crc.sql create.crc-multi.sql create.cache.sql
drop: drop.single.sql drop.multi.sql drop.crc.sql drop.crc-multi.sql drop.cache.sql

create.single.sql: create.txt cross.txt
	cat $(srcdir)/create.txt $(srcdir)/cross.txt > $@

create.multi.sql: create.txt cross.txt multi.txt
	cat $(srcdir)/create.txt $(srcdir)/cross.txt $(srcdir)/multi.txt > $@

create.crc.sql: create.txt cross-crc.txt crc.txt
	cat $(srcdir)/create.txt $(srcdir)/cross-crc.txt $(srcdir)/crc.txt > $@

create.crc-multi.sql: create.txt cross-crc.txt crc-multi.txt
	cat $(srcdir)/create.txt $(srcdir)/cross-crc.txt $(srcdir)/crc-multi.txt > $@

create.cache.sql: create.txt cross-crc.txt
	cat $(srcdir)/create.txt $(srcdir)/cross-crc.txt > $@


drop.single.sql: drop.txt drop.cross.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.cross.txt > $@

drop.multi.sql: drop.txt drop.cross.txt drop.multi.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.cross.txt $(srcdir)/drop.multi.txt > $@

drop.crc.sql: drop.txt drop.cross-crc.txt drop.crc.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.cross-crc.txt $(srcdir)/drop.crc.txt > $@

drop.crc-multi.sql: drop.txt drop.cross-crc.txt drop.crc-multi.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.cross-crc.txt $(srcdir)/drop.crc-multi.txt > $@

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
	$(mkinstalldirs) $(DESTDIR)$(dpstodir)/pgsql
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$(dpstodir)/pgsql/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$(dpstodir)/pgsql/$$p; \
	done
	-rmdir $(DESTDIR)$(dpstodir)/pgsql

