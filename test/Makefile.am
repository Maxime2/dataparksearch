TESTS = dps_test-run

check_PROGRAMS = dps_test
dps_test_SOURCES = dps_test.c

CLEANFILES = dps_test dps_test.log sqlnet.log

clean-local:
	rm -rf test-cached/var/ test-cache/var/ test-searchd/var/

test_follow_FILES = \
test-follow/indexer.conf \
test-follow/query.res \
test-follow/query.tst \
test-follow/test.cmd \
test-follow/htdocs1/test.html \
test-follow/htdocs2/test.html

test_hrefonly_FILES = \
test-hrefonly/indexer.conf \
test-hrefonly/query.res \
test-hrefonly/query.tst \
test-hrefonly/test.cmd \
test-hrefonly/htdocs/test1.html \
test-hrefonly/htdocs/test2.html \
test-hrefonly/htdocs/test3.html \
test-hrefonly/htdocs/test1.txt

test_include_FILES = \
test-include/include.conf \
test-include/indexer.conf \
test-include/query.res \
test-include/query.tst \
test-include/test.cmd \
test-include/htdocs/test1.html \
test-include/htdocs/test2.html \
test-include/htdocs/test3.html \
test-include/htdocs/test1.txt

test_mailto_FILES = \
test-mailto/indexer.conf \
test-mailto/query.res \
test-mailto/query.tst \
test-mailto/test.cmd \
test-mailto/htdocs/test.html

test_notfound_FILES = \
test-notfound/indexer.conf \
test-notfound/query.res \
test-notfound/query.tst \
test-notfound/test.cmd \
test-notfound/htdocs/test1.html \
test-notfound/htdocs/test2.html \
test-notfound/htdocs/test1.txt

test_notmodified_FILES = \
test-notmodified/indexer.conf \
test-notmodified/query.res \
test-notmodified/query.tst \
test-notmodified/test.cmd \
test-notmodified/htdocs/test1.html \
test-notmodified/htdocs/test2.html \
test-notmodified/htdocs/test3.html \
test-notmodified/htdocs/test4.html

test_quotes_FILES = \
test-quotes/indexer.conf \
test-quotes/query.res \
test-quotes/query.tst \
test-quotes/test.cmd \
test-quotes/htdocs/test1.txt

test_revalias_FILES = \
test-revalias/dict.res \
test-revalias/dict.tst \
test-revalias/indexer0.conf \
test-revalias/indexer1.conf \
test-revalias/test.cmd \
test-revalias/url.tst \
test-revalias/url0.res \
test-revalias/url1.res \
test-revalias/htdocs/a.txt \
test-revalias/htdocs/b.txt \
test-revalias/htdocs/c.txt

test_revalias1_FILES = \
test-revalias1/indexer.conf \
test-revalias1/test.cmd \
test-revalias1/query.tst \
test-revalias1/query.res \
test-revalias1/htdocs/index.html

test_multi_FILES = \
test-multi/indexer.conf \
test-multi/query.res \
test-multi/query.tst \
test-multi/test.cmd \
test-multi/search.res \
test-multi/search2.res \
test-multi/search.htm \
test-multi/htdocs/test1.html \
test-multi/htdocs/test2.html \
test-multi/htdocs/test3.html \
test-multi/htdocs/test4.html \
test-multi/htdocs/test1.txt

test_crc_multi_FILES = \
test-crc-multi/indexer.conf \
test-crc-multi/query.res \
test-crc-multi/query.tst \
test-crc-multi/test.cmd \
test-crc-multi/search.res \
test-crc-multi/search2.res \
test-crc-multi/search.htm \
test-crc-multi/htdocs/test1.html \
test-crc-multi/htdocs/test2.html \
test-crc-multi/htdocs/test3.html \
test-crc-multi/htdocs/test4.html \
test-crc-multi/htdocs/test1.txt

test_complexbody_FILES = \
test-complexbody/indexer.conf \
test-complexbody/query.res \
test-complexbody/query.tst \
test-complexbody/test.cmd \
test-complexbody/htdocs/test1.html \
test-complexbody/htdocs/test2.html

test_charset_FILES = \
test-charset/indexer.conf \
test-charset/query.res \
test-charset/query.tst \
test-charset/test.cmd \
test-charset/htdocs/segmented.gb2312.txt \
test-charset/htdocs/segmented.utf-8.txt \
test-charset/htdocs/unsegmented.gb2312.txt \
test-charset/htdocs/unsegmented.utf-8.txt \
test-charset/htdocs/JP.html \
test-charset/htdocs/ZH.html

test_sections_FILES = \
test-sections/indexer.conf \
test-sections/match.sl \
test-sections/cz.sl \
test-sections/query.res \
test-sections/query.tst \
test-sections/test.cmd \
test-sections/search.htm \
test-sections/search.res \
test-sections/search2.res \
test-sections/search3.res \
test-sections/search4.res \
test-sections/search5.res \
test-sections/search6.res \
test-sections/search7.res \
test-sections/htdocs/test1.html \
test-sections/htdocs/test2.html \
test-sections/htdocs/test.pdf \
test-sections/htdocs/test.xml \
test-sections/htdocs/directory/test.html \
test-sections/htdocs/directory/test-tz.html


test_sample_FILES = \
test-sample/indexer.conf \
test-sample/query.res \
test-sample/query.tst \
test-sample/test.cmd \
test-sample/search.htm \
test-sample/search.res \
test-sample/search2.res \
test-sample/htdocs/test1.html \
test-sample/htdocs/test2.html \
test-sample/htdocs/test3.html \
test-sample/htdocs/test4.html \
test-sample/htdocs/test5.html \
test-sample/htdocs/test1.txt \
test-sample/htdocs/z-clone1.txt \
test-sample/htdocs/z-clone2.html

test_parsehtml_FILES = \
test-parsehtml/indexer.conf \
test-parsehtml/query.res \
test-parsehtml/json.htm \
test-parsehtml/search.htm \
test-parsehtml/search.res \
test-parsehtml/search-j.res \
test-parsehtml/search2.res \
test-parsehtml/search3.res \
test-parsehtml/query.tst \
test-parsehtml/test.cmd \
test-parsehtml/htdocs/accept.html \
test-parsehtml/htdocs/bogus-fpi.html \
test-parsehtml/htdocs/display.html \
test-parsehtml/htdocs/html40-frameset.html \
test-parsehtml/htdocs/html40-strict.html \
test-parsehtml/htdocs/html40-transitional.html \
test-parsehtml/htdocs/ispattern.html \
test-parsehtml/htdocs/meta-ua.html \
test-parsehtml/htdocs/no-newlines.html \
test-parsehtml/htdocs/refresh.html \
test-parsehtml/htdocs/xhtml1-blank-1st-line.xhtml \
test-parsehtml/htdocs/xhtml1-missing-doctype-and-xmlns.xhtml \
test-parsehtml/htdocs/xhtml1-strict-minimal.xhtml \
test-parsehtml/htdocs/xhtml1-strict-missing-xmlns.xhtml \
test-parsehtml/htdocs/xhtml1-strict.xhtml \
test-parsehtml/htdocs/index.html

test_boolean_FILES = \
test-boolean/indexer.conf \
test-boolean/query.res \
test-boolean/query.tst \
test-boolean/test.cmd \
test-boolean/search.htm \
test-boolean/search.res \
test-boolean/search2.res \
test-boolean/search3.res \
test-boolean/search4.res \
test-boolean/search5.res \
test-boolean/search6.res \
test-boolean/search7.res \
test-boolean/search8.res \
test-boolean/acr.conf \
test-boolean/htdocs/anyword.txt \
test-boolean/htdocs/body1.txt \
test-boolean/htdocs/body2.txt \
test-boolean/htdocs/body3.txt \
test-boolean/htdocs/body4.txt \
test-boolean/htdocs/body5.txt \
test-boolean/htdocs/body6.txt \
test-boolean/htdocs/body7.txt \
test-boolean/htdocs/body8.txt \
test-boolean/htdocs/body9.txt \
test-boolean/htdocs/body10.txt \
test-boolean/htdocs/body11.txt \
test-boolean/htdocs/body12.txt \
test-boolean/htdocs/body13.txt \
test-boolean/htdocs/body14.txt \
test-boolean/htdocs/body15.txt \
test-boolean/htdocs/body16.txt \
test-boolean/htdocs/body17.txt \
test-boolean/htdocs/body18.txt \
test-boolean/htdocs/body19.txt \
test-boolean/htdocs/body20.txt \
test-boolean/htdocs/body-not-near.txt


test_cache_FILES =\
test-cache/htdocs/ispattern.html \
test-cache/htdocs/test.html \
test-cache/htdocs/testpage1.html \
test-cache/htdocs/testpage2.html \
test-cache/htdocs/testpage3.html \
test-cache/htdocs/testpage4.html \
test-cache/htdocs/testpage5.html \
test-cache/htdocs/testpage6.html \
test-cache/htdocs/testpage7.html \
test-cache/htdocs/testpage8.html \
test-cache/htdocs/testpage9.html \
test-cache/htdocs/testpage10.html \
test-cache/htdocs/testpage11.html \
test-cache/htdocs/testpage12.html \
test-cache/htdocs/testpage13.html \
test-cache/indexer.conf \
test-cache/query.res \
test-cache/query.tst \
test-cache/test.cmd \
test-cache/search.htm \
test-cache/search.res \
test-cache/search2.res \
test-cache/search3.res \
test-cache/search4.res \
test-cache/search5.res \
test-cache/search6.res 


test_cached_FILES =\
test-cached/htdocs/ispattern.html \
test-cached/htdocs/testpage1.html \
test-cached/htdocs/testpage2.html \
test-cached/htdocs/testpage3.html \
test-cached/htdocs/testpage4.html \
test-cached/htdocs/testpage5.html \
test-cached/htdocs/testpage6.html \
test-cached/htdocs/testpage7.html \
test-cached/htdocs/testpage8.html \
test-cached/htdocs/testpage9.html \
test-cached/htdocs/testpage10.html \
test-cached/htdocs/testpage11.html \
test-cached/htdocs/testpage12.html \
test-cached/htdocs/testpage13.html \
test-cached/indexer.conf \
test-cached/cached.conf \
test-cached/query.res \
test-cached/query.tst \
test-cached/test.cmd \
test-cached/search.htm \
test-cached/search.res \
test-cached/search2.res


test_searchd_FILES =\
test-searchd/htdocs/testpage1.html \
test-searchd/htdocs/testpage2.html \
test-searchd/htdocs/testpage3.html \
test-searchd/htdocs/testpage4.html \
test-searchd/htdocs/testpage5.html \
test-searchd/htdocs/testpage6.html \
test-searchd/htdocs/testpage7.html \
test-searchd/htdocs/testpage8.html \
test-searchd/htdocs/testpage9.html \
test-searchd/htdocs/testpage10.html \
test-searchd/htdocs/testpage11.html \
test-searchd/htdocs/testpage12.html \
test-searchd/htdocs/testpage13.html \
test-searchd/indexer.conf \
test-searchd/searchd.conf \
test-searchd/query.res \
test-searchd/query.tst \
test-searchd/test.cmd \
test-searchd/search.htm \
test-searchd/search.res \
test-searchd/search2.res \
test-searchd/search3.res



EXTRA_DIST = dps_test-run \
 ${test_follow_FILES}   \
 ${test_hrefonly_FILES} \
 ${test_include_FILES}  \
 ${test_mailto_FILES}   \
 ${test_notfound_FILES}   \
 ${test_notmodified_FILES}   \
 ${test_quotes_FILES}   \
 ${test_revalias_FILES} \
 ${test_revalias1_FILES} \
 ${test_multi_FILES} \
 ${test_crc_multi_FILES} \
 ${test_complexbody_FILES} \
 ${test_charset_FILES} \
 ${test_sections_FILES} \
 ${test_sample_FILES} \
 ${test_boolean_FILES} \
 ${test_parsehtml_FILES} \
 ${test_cache_FILES} \
 ${test_cached_FILES} \
 ${test_searchd_FILES}


