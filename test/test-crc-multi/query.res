SQL>'FIELDS=OFF'
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	272	0	-2059195102	http://site/
200	198	1	-1865365000	http://site/test4.html
200	817	1	-1755844888	http://site/test1.html
200	122	1	-271113602	http://site/test3.html
200	164	1	445670373	http://site/test2.html
200	87	1	2035244578	http://site/test1.txt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-2059195102	http://site/	body	 test1.html  test1.txt  test3.html  test4.html  .  test2.html  .. 
200	-2059195102	http://site/	charset	ISO-8859-1
200	-2059195102	http://site/	content-language	en
200	-2059195102	http://site/	content-type	text/html
200	-2059195102	http://site/	title	http://site/
200	-1865365000	http://site/test4.html	body	 This is the 4th test page. Here is the third one. проверка виндовой кодироки общественные
200	-1865365000	http://site/test4.html	charset	windows-1251
200	-1865365000	http://site/test4.html	content-language	ru
200	-1865365000	http://site/test4.html	content-type	text/html
200	-1865365000	http://site/test4.html	title	Test 4 title
200	-1755844888	http://site/test1.html	body	 This is the first test page. Here is the second one. 1 22 333 4444 55555 666666 7777777 88888888 999999999 AAAAAAAAAA BBBBBBBBBBB CCCCCCCCCCCC DDDDDDDDDDDDD EEEEEEEEEEEEEE FFFFFFFFFFFFFFF GGGGGGGGGGGGGGGG HHHHHHHHHHHHHHHHH IIIIIIIIIIIIIIIIII JJJJJJJJJJJJJJ
200	-1755844888	http://site/test1.html	charset	ISO-8859-1
200	-1755844888	http://site/test1.html	content-language	en
200	-1755844888	http://site/test1.html	content-type	text/html
200	-1755844888	http://site/test1.html	title	Test 1 title
200	-271113602	http://site/test3.html	body	 This is the third test. No more tests available.
200	-271113602	http://site/test3.html	charset	ISO-8859-1
200	-271113602	http://site/test3.html	content-language	en
200	-271113602	http://site/test3.html	content-type	text/html
200	-271113602	http://site/test3.html	title	Test 3 title
200	445670373	http://site/test2.html	body	 This is the second test page. Here is the third one. ÏÂÝÅÓÔ×ÅÎÎÙÅ
200	445670373	http://site/test2.html	charset	ISO-8859-1
200	445670373	http://site/test2.html	content-language	en
200	445670373	http://site/test2.html	content-type	text/html
200	445670373	http://site/test2.html	title	Test 2 title
200	2035244578	http://site/test1.txt	body	This is a text file. No title available. abcdefghijklmnopqrstuvwxyz Ford motor company
200	2035244578	http://site/test1.txt	charset	ISO-8859-1
200	2035244578	http://site/test1.txt	content-language	en
200	2035244578	http://site/test1.txt	content-type	text/plain
SQL>SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict3  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-159569284	196867	-2059195102	http://site/
-1946824460	393475	-1865365000	http://site/test4.html
1494415877	459011	-1865365000	http://site/test4.html
-1946824460	786691	-1865365000	http://site/test4.html
101749011	917763	-1865365000	http://site/test4.html
-1946824460	393475	-1755844888	http://site/test1.html
-1946824460	786691	-1755844888	http://site/test1.html
101749011	917763	-1755844888	http://site/test1.html
998613904	1114371	-1755844888	http://site/test1.html
-1946824460	393475	-271113602	http://site/test3.html
-1946824460	393475	445670373	http://site/test2.html
-1946824460	786691	445670373	http://site/test2.html
101749011	917763	445670373	http://site/test2.html
-159569284	984579	2035244578	http://site/test1.txt
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict4  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-569393558	66052	-2059195102	http://site/
1765998435	66052	-2059195102	http://site/
-1006712810	131332	-2059195102	http://site/
-1006712810	262404	-2059195102	http://site/
-1006712810	327940	-2059195102	http://site/
-1006712810	393476	-2059195102	http://site/
-569393558	461060	-2059195102	http://site/
1765998435	526340	-2059195102	http://site/
-1621694977	66052	-1865365000	http://site/test4.html
1754423178	262404	-1865365000	http://site/test4.html
-1621694977	524548	-1865365000	http://site/test4.html
453442368	590084	-1865365000	http://site/test4.html
1002626126	655620	-1865365000	http://site/test4.html
-545081538	917764	-1865365000	http://site/test4.html
-569393558	1247492	-1865365000	http://site/test4.html
1765998435	1312772	-1865365000	http://site/test4.html
-1006712810	1377796	-1865365000	http://site/test4.html
-1621694977	66052	-1755844888	http://site/test1.html
1754423178	262404	-1755844888	http://site/test1.html
-1621694977	524548	-1755844888	http://site/test1.html
453442368	590084	-1755844888	http://site/test1.html
1002626126	655620	-1755844888	http://site/test1.html
-545081538	917764	-1755844888	http://site/test1.html
1606940738	1179908	-1755844888	http://site/test1.html
-569393558	3082500	-1755844888	http://site/test1.html
1765998435	3147780	-1755844888	http://site/test1.html
-1006712810	3212804	-1755844888	http://site/test1.html
-1621694977	66052	-271113602	http://site/test3.html
1754423178	262404	-271113602	http://site/test3.html
-1621694977	524548	-271113602	http://site/test3.html
929694424	655620	-271113602	http://site/test3.html
-569393558	854276	-271113602	http://site/test3.html
1765998435	919556	-271113602	http://site/test3.html
-1006712810	984580	-271113602	http://site/test3.html
-1621694977	66052	445670373	http://site/test2.html
1754423178	262404	445670373	http://site/test2.html
-1621694977	524548	445670373	http://site/test2.html
453442368	590084	445670373	http://site/test2.html
1002626126	655620	445670373	http://site/test2.html
-545081538	917764	445670373	http://site/test2.html
-569393558	1050884	445670373	http://site/test2.html
1765998435	1116164	445670373	http://site/test2.html
-1006712810	1181188	445670373	http://site/test2.html
1754423178	65796	2035244578	http://site/test1.txt
-155111828	262404	2035244578	http://site/test1.txt
1407588926	327940	2035244578	http://site/test1.txt
-376649042	655620	2035244578	http://site/test1.txt
-569393558	854276	2035244578	http://site/test1.txt
1765998435	919556	2035244578	http://site/test1.txt
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict5  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-1851156983	131333	-2059195102	http://site/
-1851156983	196869	-2059195102	http://site/
-1735645336	262405	-2059195102	http://site/
-12150064	327941	-2059195102	http://site/
303906459	393477	-2059195102	http://site/
-1175441599	197125	-1865365000	http://site/test4.html
1455614667	590085	-1865365000	http://site/test4.html
-871934223	852229	-1865365000	http://site/test4.html
-12150064	1377797	-1865365000	http://site/test4.html
-1175441599	197125	-1755844888	http://site/test1.html
-1181119014	459013	-1755844888	http://site/test1.html
1455614667	590085	-1755844888	http://site/test1.html
2010520316	1245445	-1755844888	http://site/test1.html
-1851156983	3212805	-1755844888	http://site/test1.html
-1175441599	197125	-271113602	http://site/test3.html
-871934223	459013	-271113602	http://site/test3.html
-2002111504	524549	-271113602	http://site/test3.html
1805550847	721157	-271113602	http://site/test3.html
-1735645336	984581	-271113602	http://site/test3.html
-1175441599	197125	445670373	http://site/test2.html
1455614667	590085	445670373	http://site/test2.html
-871934223	852229	445670373	http://site/test2.html
303906459	1181189	445670373	http://site/test2.html
817629237	327941	2035244578	http://site/test1.txt
-1175441599	459013	2035244578	http://site/test1.txt
1062328823	721157	2035244578	http://site/test1.txt
-1851156983	984581	2035244578	http://site/test1.txt
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict6  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-1749904744	852230	-1755844888	http://site/test1.html
427731906	1310982	-1755844888	http://site/test1.html
-1749904744	459014	445670373	http://site/test2.html
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict7  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
1310639951	1376519	-1755844888	http://site/test1.html
-2122482874	786695	2035244578	http://site/test1.txt
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict8  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
1013740972	1442056	-1755844888	http://site/test1.html
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict9  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-558105807	196873	-2059195102	http://site/
873312523	1507593	-1755844888	http://site/test1.html
761627730	786697	-271113602	http://site/test3.html
761627730	524553	2035244578	http://site/test1.txt
-558105807	984585	2035244578	http://site/test1.txt
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict10 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-599042163	131338	-2059195102	http://site/
1929661364	262410	-2059195102	http://site/
1550301829	327946	-2059195102	http://site/
-815697129	393482	-2059195102	http://site/
1550301829	1377802	-1865365000	http://site/test4.html
-327400087	1573130	-1755844888	http://site/test1.html
-599042163	3212810	-1755844888	http://site/test1.html
-1926214505	786698	-271113602	http://site/test3.html
1929661364	984586	-271113602	http://site/test3.html
-815697129	1181194	445670373	http://site/test2.html
-1926214505	524554	2035244578	http://site/test1.txt
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict11 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-853945909	1638667	-1755844888	http://site/test1.html
SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict12 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
614987914	66060	-2059195102	http://site/
995931085	1704204	-1755844888	http://site/test1.html
SQL>SQL>'SELECT d.word_id,d.intag,u.crc32,u.url FROM ndict32 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
-1713976050	1179916	-1865365000	http://site/test4.html
1236750597	2031889	-1755844888	http://site/test1.html
-1068169142	2097426	-1755844888	http://site/test1.html
1908161808	2162963	-1755844888	http://site/test1.html
-472357562	2228500	-1755844888	http://site/test1.html
92410486	2294037	-1755844888	http://site/test1.html
118018630	2359574	-1755844888	http://site/test1.html
-2108223933	2425111	-1755844888	http://site/test1.html
1000153478	2490648	-1755844888	http://site/test1.html
-1497776927	2556185	-1755844888	http://site/test1.html
-687355354	2621722	-1755844888	http://site/test1.html
1061008373	2687259	-1755844888	http://site/test1.html
-1623098498	2752796	-1755844888	http://site/test1.html
-432801041	2818333	-1755844888	http://site/test1.html
1506451081	2883870	-1755844888	http://site/test1.html
788539575	2949407	-1755844888	http://site/test1.html
817126565	3014944	-1755844888	http://site/test1.html
-2033705536	983308	445670373	http://site/test2.html
-1787711085	590106	2035244578	http://site/test1.txt
SQL>
