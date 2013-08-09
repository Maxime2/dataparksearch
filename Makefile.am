## Process this file with automake to produce Makefile.in 
ACLOCAL_AMFLAGS = -I build/m4
AUTOMAKE_OPTIONS = foreign no-installinfo dist-bzip2 no-dist-gzip
SUBDIRS = build include src doc etc create misc scripts test #php @BINDINGS@
#DIST_SUBDIRS = php java perl

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/splitter/
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/tree/
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/cache/
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/store/
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/url/
	chmod 777 $(DESTDIR)$(localstatedir)/cache/

uninstall-local:
	-rmdir $(DESTDIR)$(localstatedir)/splitter/
	-rmdir $(DESTDIR)$(localstatedir)/tree/
	-rmdir $(DESTDIR)$(localstatedir)/cache/
	-rmdir $(DESTDIR)$(localstatedir)/store/
	-rmdir $(DESTDIR)$(localstatedir)/url/
	-rmdir $(DESTDIR)$(datadir)
	-rmdir $(DESTDIR)$(sysconfdir)
	-rmdir $(DESTDIR)$(mandir)/man1
	-rmdir $(DESTDIR)$(mandir)/man5
	-rmdir $(DESTDIR)$(mandir)
	-rmdir $(DESTDIR)$(prefix)/doc
	-rmdir $(DESTDIR)$(localstatedir)
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(sbindir)
	-rmdir $(DESTDIR)$(libdir)
	-rmdir $(DESTDIR)$(includedir)
	-rmdir $(DESTDIR)$(datadir)
	-rmdir $(DESTDIR)$(prefix)

bin-dist:
	$(MAKE) install DESTDIR=$(CDIR)/$(PACKAGE)-$(VERSION)
	cd $(CDIR)/$(PACKAGE)-$(VERSION)$(prefix) \
		&& tar -ycPf $(PACKAGE)-$(VERSION)-$(DBTYPE)-$(host_triplet)-$(LINKING).tar.bz2 ./
	mv $(CDIR)/$(PACKAGE)-$(VERSION)$(prefix)/*.tar.bz2 $(CDIR)
	rm -r $(CDIR)/$(PACKAGE)-$(VERSION)

EXTRA_DIST = install.pl bootstrap acinclude.m4 README.XML README.md BUGS LICENSE \
	perl/Makefile.PL.in perl/BUGS perl/README perl/INSTALL perl/test.pl perl/Dataparksearch.pm perl/Dataparksearch.xs \
	perl/cat_ed/*.pl perl/cat_ed/*.gif perl/examples/* charset-Makefile.am charset-configure.in \
	dp-Makefile.am dp-configure.in


libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

#dist-hook:
#	cp -rf $(srcdir)/debian $(distdir)
#	cp -rf $(srcdir)/m4 $(distdir)

## libdpcharset distribution

CHARSETDISTFILES =  src/uconv.c src/unidata.c src/sgml.c src/unicode.c src/dpconv.c \
src/uconv-8bit.c src/uconv-alias.c src/uconv-big5.c src/uconv-big5hkscs.c src/uconv-eucjp.c src/uconv-euckr.c \
src/uconv-gb2312.c src/uconv-gbk.c src/uconv-sjis.c src/uconv-sys.c src/uconv-utf8.c src/uconv-gujarati.c \
src/uconv-tscii.c src/charset-utils.c src/xmalloc.c src/sgml.entities src/sgml.inc src/uconv-8bit.inc src/unidata.ch \
include/dps_config.h.in include/dps_uniconv.h include/dps_sgml.h include/dps_unicode.h \
include/dps_unidata.h src/uconv-8bit.h include/dps_charsetutils.h include/dps_filence.h include/dps_xmalloc.h \
LICENSE

charsetdistdir = libdpcharset-$(VERSION)

charsetdistdir: $(CHARSETDISTFILES)
	-chmod -R a+w $(charsetdistdir) >/dev/null 2>&1; rm -rf $(charsetdistdir)
	mkdir $(charsetdistdir)
	$(mkinstalldirs) $(charsetdistdir)/. $(charsetdistdir)/././include $(charsetdistdir)/./include $(charsetdistdir)/src
	@for file in $(CHARSETDISTFILES); do \
	  if test -f $$file; then d=.; else d=$(srcdir); fi; \
	  dir=`echo "$$file" | sed -e 's,/[^/]*$$,,'`; \
	  if test "$$dir" != "$$file" && test "$$dir" != "."; then \
	    $(mkinstalldirs) "$(charsetdistdir)/$$dir"; \
	  fi; \
	  if test -d $$d/$$file; then \
	    cp -pR $$d/$$file $(charsetdistdir) \
	    || exit 1; \
	  else \
	    test -f $(charsetdistdir)/$$file \
	    || cp -p $$d/$$file $(charsetdistdir)/$$file \
	    || exit 1; \
	  fi; \
	done
	cp bootstrap $(charsetdistdir)
	cp charset-configure.in $(charsetdistdir)/configure.in
	cp charset-Makefile.am $(charsetdistdir)/Makefile.am
	cp src/charset-Makefile.am $(charsetdistdir)/src/Makefile.am
	-find $(charsetdistdir) -type d ! -perm -777 -exec chmod a+rwx {} \; -o \
	  ! -type d ! -perm -444 -links 1 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -400 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -444 -exec $(SHELL) $(install_sh) -c -m a+r {} {} \; \
	|| chmod -R a+r $(charsetdistdir)


charset-dist: charsetdistdir
	$(AMTAR) chof - $(charsetdistdir) | GZIP=$(GZIP_ENV) gzip -c >$(charsetdistdir).tar.gz
	-chmod -R a+w $(charsetdistdir) >/dev/null 2>&1; rm -rf $(charsetdistdir)


## libdp distribution

DPDISTFILES =  src/dp.c src/dps_memcpy.inc include/dps_config.h.in LICENSE

dpdistdir = libdp-$(VERSION)

dpdistdir: $(DPDISTFILES)
	-chmod -R a+w $(dpdistdir) >/dev/null 2>&1; rm -rf $(dpdistdir)
	mkdir $(dpdistdir)
	$(mkinstalldirs) $(dpdistdir)/. $(dpdistdir)/././include $(dpdistdir)/./include $(dpdistdir)/src
	@for file in $(DPDISTFILES); do \
	  if test -f $$file; then d=.; else d=$(srcdir); fi; \
	  dir=`echo "$$file" | sed -e 's,/[^/]*$$,,'`; \
	  if test "$$dir" != "$$file" && test "$$dir" != "."; then \
	    $(mkinstalldirs) "$(dpdistdir)/$$dir"; \
	  fi; \
	  if test -d $$d/$$file; then \
	    cp -pR $$d/$$file $(dpdistdir) \
	    || exit 1; \
	  else \
	    test -f $(dpdistdir)/$$file \
	    || cp -p $$d/$$file $(dpdistdir)/$$file \
	    || exit 1; \
	  fi; \
	done
	cp bootstrap $(dpdistdir)
	cp dp-configure.in $(dpdistdir)/configure.in
	cp dp-Makefile.am $(dpdistdir)/Makefile.am
	cp src/dp-Makefile.am $(dpdistdir)/src/Makefile.am
	-find $(dpdistdir) -type d ! -perm -777 -exec chmod a+rwx {} \; -o \
	  ! -type d ! -perm -444 -links 1 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -400 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -444 -exec $(SHELL) $(install_sh) -c -m a+r {} {} \; \
	|| chmod -R a+r $(dpdistdir)


dp-dist: dpdistdir
	$(AMTAR) chof - $(dpdistdir) | GZIP=$(GZIP_ENV) gzip -c >$(dpdistdir).tar.gz
	-chmod -R a+w $(dpdistdir) >/dev/null 2>&1; rm -rf $(dpdistdir)


# Create a deb package
TMPDIR := $(shell mktemp -dtu)
PKGDATE := $(shell date -R)
pkg-deb: dist
	- rm -r $(TMPDIR)
	mkdir -p $(TMPDIR)
	cp $(PACKAGE)-$(VERSION).tar.bz2 $(TMPDIR)/
	tar --directory $(TMPDIR)/ \
		-xjf $(TMPDIR)/$(PACKAGE)-$(VERSION).tar.bz2
	cd $(TMPDIR)/$(PACKAGE)-$(VERSION)/; \
		echo | dh_make --single --copyright gpl -e myemailaddress \
			--native -f ../$(PACKAGE)-$(VERSION).tar.bz2
	cp build/debian/* $(TMPDIR)/$(PACKAGE)-$(VERSION)/debian/
	echo "$(PACKAGE) ($(VERSION)-1) UNRELEASED; urgency=low" > $(TMPDIR)/$(PACKAGE)-$(VERSION)/debian/changelog
	echo "" >> $(TMPDIR)/$(PACKAGE)-$(VERSION)/debian/changelog
	echo "  * Updating to $(VERSION)" >> $(TMPDIR)/$(PACKAGE)-$(VERSION)/debian/changelog
	echo "" >> $(TMPDIR)/$(PACKAGE)-$(VERSION)/debian/changelog
	echo " -- Maxim Zakharov <dp.maxime@gmail.com>  $(PKGDATE)" >> $(TMPDIR)/$(PACKAGE)-$(VERSION)/debian/changelog
	cd $(TMPDIR)/$(PACKAGE)-$(VERSION)/; \
		rm debian/README* debian/*.ex debian/*.EX; \
		./configure; \
		dpkg-buildpackage -rfakeroot; \
                mv ../*.deb $(PWD)
	rm -rf $(TMPDIR)

# Create an RPM package
pkg-rpm: pkg-deb
	alien --to-rpm --scripts *.deb
#pkg-rpm: dist
#	mkdir -p $(HOME)/.rpm/RPMS/i386
#	mkdir -p $(HOME)/.rpm/SRPMS
#	mkdir -p $(HOME)/.rpm/BUILD
#	mkdir -p $(HOME)/.rpm/SOURCES
#	mkdir -p $(HOME)/.rpm/tmp
#	- rm -r $(HOME)/.rpm/BUILD/$(PACKAGE)-root
#	- rm -r $(HOME)/.rpm/RPMS/$(PACKAGE)-*
#	- rm -r $(HOME)/.rpm/SRPMS/$(PACKAGE)-*
#	- rm -r $(HOME)/.rpm/SOURCES/$(PACKAGE)-*
#	cp $(PACKAGE)-$(VERSION).tar.bz2 $(HOME)/.rpm/SOURCES/
#	rpmbuild --quiet --nodeps --define="_topdir $(HOME)/.rpm" \
#		-ba build/dpsearch.spec
#	mv $(HOME)/.rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm ./

pkg-dist: pkg-deb #pkg-rpm
