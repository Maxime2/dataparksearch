SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
test1	65797	-2012557857	http://site/test1.html
test1	131333	-2012557857	http://site/test1.html
test1	196869	-2012557857	http://site/test1.html
http	264452	-2012557857	http://site/test1.html
site	329732	-2012557857	http://site/test1.html
html	394756	-2012557857	http://site/test1.html
test1	394757	-2012557857	http://site/test1.html
test1.html	394762	-2012557857	http://site/test1.html
test3	65797	-677607177	http://site/test3.html
test3	131333	-677607177	http://site/test3.html
test3	196869	-677607177	http://site/test3.html
test3	262405	-677607177	http://site/test3.html
test3	327941	-677607177	http://site/test3.html
http	395524	-677607177	http://site/test3.html
site	460804	-677607177	http://site/test3.html
html	525828	-677607177	http://site/test3.html
test3	525829	-677607177	http://site/test3.html
test3.html	525834	-677607177	http://site/test3.html
http	66052	-352834897	http://site/
site	66052	-352834897	http://site/
http://site/	66060	-352834897	http://site/
html	131332	-352834897	http://site/
test1	131333	-352834897	http://site/
test1.html	131338	-352834897	http://site/
html	196868	-352834897	http://site/
test3	196869	-352834897	http://site/
test3.html	196874	-352834897	http://site/
html	262404	-352834897	http://site/
test4	262405	-352834897	http://site/
test4.html	262410	-352834897	http://site/
html	327940	-352834897	http://site/
test2	327941	-352834897	http://site/
test2.html	327946	-352834897	http://site/
http	395524	-352834897	http://site/
site	460804	-352834897	http://site/
test2	65797	-269291223	http://site/test2.html
test2	131333	-269291223	http://site/test2.html
test2	196869	-269291223	http://site/test2.html
test2	262405	-269291223	http://site/test2.html
http	329988	-269291223	http://site/test2.html
site	395268	-269291223	http://site/test2.html
html	460292	-269291223	http://site/test2.html
test2	460293	-269291223	http://site/test2.html
test2.html	460298	-269291223	http://site/test2.html
test	66052	1562739290	http://site/test4.html
4	131585	1562739290	http://site/test4.html
test4	196869	1562739290	http://site/test4.html
http	264452	1562739290	http://site/test4.html
site	329732	1562739290	http://site/test4.html
html	394756	1562739290	http://site/test4.html
test4	394757	1562739290	http://site/test4.html
test4.html	394762	1562739290	http://site/test4.html
SQL>'SELECT status, docsize, hops, crc32, site_id, server_id, last_mod_time, url FROM url ORDER BY status, crc32, hops'
200	238	0	-352834897	614987914	614987914	0	http://site/
304	50	1	-2012557857	614987914	614987914	1333157702	http://site/test1.html
304	62	1	-677607177	614987914	614987914	1333157702	http://site/test3.html
304	56	1	-269291223	614987914	614987914	1333157702	http://site/test2.html
304	130	1	1562739290	614987914	614987914	1333157702	http://site/test4.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-352834897	http://site/	body	 test1.html  test3.html  test4.html  .  test2.html  .. 
200	-352834897	http://site/	charset	ISO-8859-1
200	-352834897	http://site/	content-language	en
200	-352834897	http://site/	content-type	text/html
200	-352834897	http://site/	title	http://site/
304	-2012557857	http://site/test1.html	body	test1 test1 test1
304	-2012557857	http://site/test1.html	charset	ISO-8859-1
304	-2012557857	http://site/test1.html	content-language	en
304	-2012557857	http://site/test1.html	content-type	text/html
304	-677607177	http://site/test3.html	body	test3 test3 test3 test3 test3
304	-677607177	http://site/test3.html	charset	ISO-8859-1
304	-677607177	http://site/test3.html	content-language	en
304	-677607177	http://site/test3.html	content-type	text/html
304	-269291223	http://site/test2.html	body	test2 test2 test2 test2
304	-269291223	http://site/test2.html	charset	ISO-8859-1
304	-269291223	http://site/test2.html	content-language	en
304	-269291223	http://site/test2.html	content-type	text/html
304	1562739290	http://site/test4.html	body	 test4
304	1562739290	http://site/test4.html	charset	ISO-8859-1
304	1562739290	http://site/test4.html	content-language	en
304	1562739290	http://site/test4.html	content-type	text/html
304	1562739290	http://site/test4.html	title	Test 4
SQL>'SELECT ot,k FROM links ORDER BY ot,k'
1	1
1	2
1	3
1	4
1	5
SQL>SQL>
