SQL>'FIELDS=OFF'
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	198	1	-2114798719	http://site/test4.html
200	272	0	-2059195102	http://site/
200	817	1	-1755844888	http://site/test1.html
200	164	1	-798181598	http://site/test2.html
200	122	1	-271113602	http://site/test3.html
200	87	1	2035244578	http://site/test1.txt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-2114798719	http://site/test4.html	body	 This is the 4th test page. Here is the third one. ОПНБЕПЙЮ БХМДНБНИ ЙНДХПНЙХ НАЫЕЯРБЕММШЕ
200	-2114798719	http://site/test4.html	charset	KOI8-R
200	-2114798719	http://site/test4.html	content-language	ru
200	-2114798719	http://site/test4.html	content-type	text/html
200	-2114798719	http://site/test4.html	title	Test 4 title
200	-2059195102	http://site/	body	 test1.html  test1.txt  test3.html  test4.html  .  test2.html  .. 
200	-2059195102	http://site/	charset	KOI8-R
200	-2059195102	http://site/	content-language	ru
200	-2059195102	http://site/	content-type	text/html
200	-2059195102	http://site/	title	http://site/
200	-1755844888	http://site/test1.html	body	 This is the first test page. Here is the second one. 1 22 333 4444 55555 666666 7777777 88888888 999999999 AAAAAAAAAA BBBBBBBBBBB CCCCCCCCCCCC DDDDDDDDDDDDD EEEEEEEEEEEEEE FFFFFFFFFFFFFFF GGGGGGGGGGGGGGGG HHHHHHHHHHHHHHHHH IIIIIIIIIIIIIIIIII JJJJJJJJJJJJJJ
200	-1755844888	http://site/test1.html	charset	KOI8-R
200	-1755844888	http://site/test1.html	content-language	ru
200	-1755844888	http://site/test1.html	content-type	text/html
200	-1755844888	http://site/test1.html	title	Test 1 title
200	-798181598	http://site/test2.html	body	 This is the second test page. Here is the third one. общественные
200	-798181598	http://site/test2.html	charset	KOI8-R
200	-798181598	http://site/test2.html	content-language	ru
200	-798181598	http://site/test2.html	content-type	text/html
200	-798181598	http://site/test2.html	title	Test 2 title
200	-271113602	http://site/test3.html	body	 This is the third test. No more tests available.
200	-271113602	http://site/test3.html	charset	KOI8-R
200	-271113602	http://site/test3.html	content-language	ru
200	-271113602	http://site/test3.html	content-type	text/html
200	-271113602	http://site/test3.html	title	Test 3 title
200	2035244578	http://site/test1.txt	body	This is a text file. No title available. abcdefghijklmnopqrstuvwxyz Ford motor company
200	2035244578	http://site/test1.txt	charset	KOI8-R
200	2035244578	http://site/test1.txt	content-language	ru
200	2035244578	http://site/test1.txt	content-type	text/plain
SQL>SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict3  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
the	393475	-2114798719	http://site/test4.html
4th	459011	-2114798719	http://site/test4.html
the	786691	-2114798719	http://site/test4.html
one	917763	-2114798719	http://site/test4.html
txt	196867	-2059195102	http://site/
the	393475	-1755844888	http://site/test1.html
the	786691	-1755844888	http://site/test1.html
one	917763	-1755844888	http://site/test1.html
333	1114371	-1755844888	http://site/test1.html
the	393475	-798181598	http://site/test2.html
the	786691	-798181598	http://site/test2.html
one	917763	-798181598	http://site/test2.html
the	393475	-271113602	http://site/test3.html
txt	984579	2035244578	http://site/test1.txt
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict4  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
test	66052	-2114798719	http://site/test4.html
this	262404	-2114798719	http://site/test4.html
test	524548	-2114798719	http://site/test4.html
page	590084	-2114798719	http://site/test4.html
here	655620	-2114798719	http://site/test4.html
one.	917764	-2114798719	http://site/test4.html
http	1247492	-2114798719	http://site/test4.html
site	1312772	-2114798719	http://site/test4.html
html	1377796	-2114798719	http://site/test4.html
site	66052	-2059195102	http://site/
http	66052	-2059195102	http://site/
html	131332	-2059195102	http://site/
html	262404	-2059195102	http://site/
html	327940	-2059195102	http://site/
html	393476	-2059195102	http://site/
http	461060	-2059195102	http://site/
site	526340	-2059195102	http://site/
test	66052	-1755844888	http://site/test1.html
this	262404	-1755844888	http://site/test1.html
test	524548	-1755844888	http://site/test1.html
page	590084	-1755844888	http://site/test1.html
here	655620	-1755844888	http://site/test1.html
one.	917764	-1755844888	http://site/test1.html
4444	1179908	-1755844888	http://site/test1.html
http	3082500	-1755844888	http://site/test1.html
site	3147780	-1755844888	http://site/test1.html
html	3212804	-1755844888	http://site/test1.html
test	66052	-798181598	http://site/test2.html
this	262404	-798181598	http://site/test2.html
test	524548	-798181598	http://site/test2.html
page	590084	-798181598	http://site/test2.html
here	655620	-798181598	http://site/test2.html
one.	917764	-798181598	http://site/test2.html
http	1050884	-798181598	http://site/test2.html
site	1116164	-798181598	http://site/test2.html
html	1181188	-798181598	http://site/test2.html
test	66052	-271113602	http://site/test3.html
this	262404	-271113602	http://site/test3.html
test	524548	-271113602	http://site/test3.html
more	655620	-271113602	http://site/test3.html
http	854276	-271113602	http://site/test3.html
site	919556	-271113602	http://site/test3.html
html	984580	-271113602	http://site/test3.html
this	65796	2035244578	http://site/test1.txt
text	262404	2035244578	http://site/test1.txt
file	327940	2035244578	http://site/test1.txt
ford	655620	2035244578	http://site/test1.txt
http	854276	2035244578	http://site/test1.txt
site	919556	2035244578	http://site/test1.txt
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict5  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
title	197125	-2114798719	http://site/test4.html
page.	590085	-2114798719	http://site/test4.html
third	852229	-2114798719	http://site/test4.html
test4	1377797	-2114798719	http://site/test4.html
test1	131333	-2059195102	http://site/
test1	196869	-2059195102	http://site/
test3	262405	-2059195102	http://site/
test4	327941	-2059195102	http://site/
test2	393477	-2059195102	http://site/
title	197125	-1755844888	http://site/test1.html
first	459013	-1755844888	http://site/test1.html
page.	590085	-1755844888	http://site/test1.html
55555	1245445	-1755844888	http://site/test1.html
test1	3212805	-1755844888	http://site/test1.html
title	197125	-798181598	http://site/test2.html
page.	590085	-798181598	http://site/test2.html
third	852229	-798181598	http://site/test2.html
test2	1181189	-798181598	http://site/test2.html
title	197125	-271113602	http://site/test3.html
third	459013	-271113602	http://site/test3.html
test.	524549	-271113602	http://site/test3.html
tests	721157	-271113602	http://site/test3.html
test3	984581	-271113602	http://site/test3.html
file.	327941	2035244578	http://site/test1.txt
title	459013	2035244578	http://site/test1.txt
motor	721157	2035244578	http://site/test1.txt
test1	984581	2035244578	http://site/test1.txt
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict6  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
second	852230	-1755844888	http://site/test1.html
666666	1310982	-1755844888	http://site/test1.html
second	459014	-798181598	http://site/test2.html
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict7  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
7777777	1376519	-1755844888	http://site/test1.html
company	786695	2035244578	http://site/test1.txt
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict8  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
88888888	1442056	-1755844888	http://site/test1.html
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict9  d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
test1.txt	196873	-2059195102	http://site/
999999999	1507593	-1755844888	http://site/test1.html
available	786697	-271113602	http://site/test3.html
available	524553	2035244578	http://site/test1.txt
test1.txt	984585	2035244578	http://site/test1.txt
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict10 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
test4.html	1377802	-2114798719	http://site/test4.html
test1.html	131338	-2059195102	http://site/
test3.html	262410	-2059195102	http://site/
test4.html	327946	-2059195102	http://site/
test2.html	393482	-2059195102	http://site/
aaaaaaaaaa	1573130	-1755844888	http://site/test1.html
test1.html	3212810	-1755844888	http://site/test1.html
test2.html	1181194	-798181598	http://site/test2.html
available.	786698	-271113602	http://site/test3.html
test3.html	984586	-271113602	http://site/test3.html
available.	524554	2035244578	http://site/test1.txt
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict11 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
bbbbbbbbbbb	1638667	-1755844888	http://site/test1.html
SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict12 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
http://site/	66060	-2059195102	http://site/
cccccccccccc	1704204	-1755844888	http://site/test1.html
SQL>SQL>'SELECT d.word,d.intag,u.crc32,u.url FROM dict32 d, url u WHERE u.rec_id=d.url_id ORDER BY u.crc32,d.intag'
наыеярбеммше	1179916	-2114798719	http://site/test4.html
hhhhhhhhhhhhhhhhh	2031889	-1755844888	http://site/test1.html
iiiiiiiiiiiiiiiiii	2097426	-1755844888	http://site/test1.html
jjjjjjjjjjjjjjjjjjj	2162963	-1755844888	http://site/test1.html
kkkkkkkkkkkkkkkkkkkk	2228500	-1755844888	http://site/test1.html
lllllllllllllllllllll	2294037	-1755844888	http://site/test1.html
mmmmmmmmmmmmmmmmmmmmmm	2359574	-1755844888	http://site/test1.html
nnnnnnnnnnnnnnnnnnnnnnn	2425111	-1755844888	http://site/test1.html
oooooooooooooooooooooooo	2490648	-1755844888	http://site/test1.html
ppppppppppppppppppppppppp	2556185	-1755844888	http://site/test1.html
qqqqqqqqqqqqqqqqqqqqqqqqqq	2621722	-1755844888	http://site/test1.html
rrrrrrrrrrrrrrrrrrrrrrrrrrr	2687259	-1755844888	http://site/test1.html
ssssssssssssssssssssssssssss	2752796	-1755844888	http://site/test1.html
ttttttttttttttttttttttttttttt	2818333	-1755844888	http://site/test1.html
uuuuuuuuuuuuuuuuuuuuuuuuuuuuuu	2883870	-1755844888	http://site/test1.html
vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv	2949407	-1755844888	http://site/test1.html
wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww	3014944	-1755844888	http://site/test1.html
общественные	983308	-798181598	http://site/test2.html
abcdefghijklmnopqrstuvwxyz	590106	2035244578	http://site/test1.txt
SQL>
