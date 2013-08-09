SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
http	66052	-2017391271	http://site2/
site2	66053	-2017391271	http://site2/
http://site2/	66061	-2017391271	http://site2/
html	131332	-2017391271	http://site2/
test	131332	-2017391271	http://site2/
test.html	131337	-2017391271	http://site2/
http	66052	-1433741546	http://site1/
site1	66053	-1433741546	http://site1/
http://site1/	66061	-1433741546	http://site1/
html	131332	-1433741546	http://site1/
test	131332	-1433741546	http://site1/
test.html	131337	-1433741546	http://site1/
s2	66050	-548453559	http://site2/test.html
t2	131586	-548453559	http://site2/test.html
ss2	196867	-548453559	http://site2/test.html
b2	262402	-548453559	http://site2/test.html
b2.	262403	-548453559	http://site2/test.html
site1	327941	-548453559	http://site2/test.html
site1.	327942	-548453559	http://site2/test.html
s1	66050	1901937092	http://site1/test.html
t1	131586	1901937092	http://site1/test.html
ss1	196867	1901937092	http://site1/test.html
bb1	262403	1901937092	http://site1/test.html
bb1.	262404	1901937092	http://site1/test.html
site2	327941	1901937092	http://site1/test.html
site2.	327942	1901937092	http://site1/test.html
an	393474	1901937092	http://site1/test.html
ok	459010	1901937092	http://site1/test.html
link	524548	1901937092	http://site1/test.html
link.	524549	1901937092	http://site1/test.html
a	590081	1901937092	http://site1/test.html
stange	655622	1901937092	http://site1/test.html
link	721156	1901937092	http://site1/test.html
link.	721157	1901937092	http://site1/test.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32, hops'
200	129	2	-2017391271	http://site2/
200	129	0	-1433741546	http://site1/
200	109	3	-548453559	http://site2/test.html
200	216	1	1901937092	http://site1/test.html
404	0	2	0	http://site2/?param
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-2017391271	http://site2/	body	 .  test.html  .. 
200	-2017391271	http://site2/	charset	ISO-8859-1
200	-2017391271	http://site2/	content-language	en
200	-2017391271	http://site2/	content-type	text/html
200	-2017391271	http://site2/	title	http://site2/
200	-1433741546	http://site1/	body	 .  test.html  .. 
200	-1433741546	http://site1/	charset	ISO-8859-1
200	-1433741546	http://site1/	content-language	en
200	-1433741546	http://site1/	content-type	text/html
200	-1433741546	http://site1/	title	http://site1/
200	-548453559	http://site2/test.html	body	 Ss2 b2. Site1. 
200	-548453559	http://site2/test.html	charset	ISO-8859-1
200	-548453559	http://site2/test.html	content-language	en
200	-548453559	http://site2/test.html	content-type	text/html
200	-548453559	http://site2/test.html	title	S2 t2
200	1901937092	http://site1/test.html	body	 Ss1 bb1. Site2.  an OK link.  a stange link. 
200	1901937092	http://site1/test.html	charset	ISO-8859-1
200	1901937092	http://site1/test.html	content-language	en
200	1901937092	http://site1/test.html	content-type	text/html
200	1901937092	http://site1/test.html	title	S1 t1
SQL>SQL>
