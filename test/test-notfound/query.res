SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	65796	-1302496456	http://site/test1.txt
is	131330	-1302496456	http://site/test1.txt
a	196865	-1302496456	http://site/test1.txt
text	262404	-1302496456	http://site/test1.txt
file	327940	-1302496456	http://site/test1.txt
file.	327941	-1302496456	http://site/test1.txt
no	393474	-1302496456	http://site/test1.txt
title	459013	-1302496456	http://site/test1.txt
available	524553	-1302496456	http://site/test1.txt
available.	524554	-1302496456	http://site/test1.txt
http	592132	-1302496456	http://site/test1.txt
site	657412	-1302496456	http://site/test1.txt
txt	722435	-1302496456	http://site/test1.txt
test1	722437	-1302496456	http://site/test1.txt
test1.txt	722441	-1302496456	http://site/test1.txt
test	66052	1062648687	http://site/test2.html
2	131585	1062648687	http://site/test2.html
title	197125	1062648687	http://site/test2.html
this	262404	1062648687	http://site/test2.html
is	327938	1062648687	http://site/test2.html
the	393475	1062648687	http://site/test2.html
second	459014	1062648687	http://site/test2.html
test	524548	1062648687	http://site/test2.html
page	590084	1062648687	http://site/test2.html
page.	590085	1062648687	http://site/test2.html
here	655620	1062648687	http://site/test2.html
is	721154	1062648687	http://site/test2.html
the	786691	1062648687	http://site/test2.html
third	852229	1062648687	http://site/test2.html
one	917763	1062648687	http://site/test2.html
one.	917764	1062648687	http://site/test2.html
http	985348	1062648687	http://site/test2.html
site	1050628	1062648687	http://site/test2.html
html	1115652	1062648687	http://site/test2.html
test2	1115653	1062648687	http://site/test2.html
test2.html	1115658	1062648687	http://site/test2.html
http	66052	1307380169	http://site/
site	66052	1307380169	http://site/
http://site/	66060	1307380169	http://site/
html	131332	1307380169	http://site/
test1	131333	1307380169	http://site/
test1.html	131338	1307380169	http://site/
txt	196867	1307380169	http://site/
test1	196869	1307380169	http://site/
test1.txt	196873	1307380169	http://site/
html	262404	1307380169	http://site/
test2	262405	1307380169	http://site/
test2.html	262410	1307380169	http://site/
http	329988	1307380169	http://site/
site	395268	1307380169	http://site/
test	66052	2014493141	http://site/test1.html
1	131585	2014493141	http://site/test1.html
title	197125	2014493141	http://site/test1.html
this	262404	2014493141	http://site/test1.html
is	327938	2014493141	http://site/test1.html
the	393475	2014493141	http://site/test1.html
first	459013	2014493141	http://site/test1.html
test	524548	2014493141	http://site/test1.html
page	590084	2014493141	http://site/test1.html
page.	590085	2014493141	http://site/test1.html
here	655620	2014493141	http://site/test1.html
is	721154	2014493141	http://site/test1.html
the	786691	2014493141	http://site/test1.html
second	852230	2014493141	http://site/test1.html
one	917763	2014493141	http://site/test1.html
one.	917764	2014493141	http://site/test1.html
http	985348	2014493141	http://site/test1.html
site	1050628	2014493141	http://site/test1.html
html	1115652	2014493141	http://site/test1.html
test1	1115653	2014493141	http://site/test1.html
test1.html	1115658	2014493141	http://site/test1.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	41	1	-1302496456	http://site/test1.txt
200	151	1	1062648687	http://site/test2.html
200	200	0	1307380169	http://site/
200	151	1	2014493141	http://site/test1.html
404	0	2	0	http://site/test3.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-1302496456	http://site/test1.txt	body	This is a text file. No title available.
200	-1302496456	http://site/test1.txt	charset	ISO-8859-1
200	-1302496456	http://site/test1.txt	content-language	en
200	-1302496456	http://site/test1.txt	content-type	text/plain
200	1062648687	http://site/test2.html	body	 This is the second test page. Here is the third one. 
200	1062648687	http://site/test2.html	charset	ISO-8859-1
200	1062648687	http://site/test2.html	content-language	en
200	1062648687	http://site/test2.html	content-type	text/html
200	1062648687	http://site/test2.html	title	Test 2 title
200	1307380169	http://site/	body	 test1.html  test1.txt  .  test2.html  .. 
200	1307380169	http://site/	charset	ISO-8859-1
200	1307380169	http://site/	content-language	en
200	1307380169	http://site/	content-type	text/html
200	1307380169	http://site/	title	http://site/
200	2014493141	http://site/test1.html	body	 This is the first test page. Here is the second one. 
200	2014493141	http://site/test1.html	charset	ISO-8859-1
200	2014493141	http://site/test1.html	content-language	en
200	2014493141	http://site/test1.html	content-type	text/html
200	2014493141	http://site/test1.html	title	Test 1 title
SQL>SQL>
