SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
test	66052	62841357	http://site/test1.html
title	131589	62841357	http://site/test1.html
this	196868	62841357	http://site/test1.html
is	262402	62841357	http://site/test1.html
the	327939	62841357	http://site/test1.html
complex	393479	62841357	http://site/test1.html
body	459012	62841357	http://site/test1.html
test	524548	62841357	http://site/test1.html
page	590084	62841357	http://site/test1.html
page.	590085	62841357	http://site/test1.html
http	657668	62841357	http://site/test1.html
site	722948	62841357	http://site/test1.html
html	787972	62841357	http://site/test1.html
test1	787973	62841357	http://site/test1.html
test1.html	787978	62841357	http://site/test1.html
blabla	65798	1259310415	http://site/test2.html
blabla	131334	1259310415	http://site/test2.html
http	198916	1259310415	http://site/test2.html
site	264196	1259310415	http://site/test2.html
html	329220	1259310415	http://site/test2.html
test2	329221	1259310415	http://site/test2.html
test2.html	329226	1259310415	http://site/test2.html
site	66052	2037089450	http://site/
http	66052	2037089450	http://site/
http://site/	66060	2037089450	http://site/
html	131332	2037089450	http://site/
test1	131333	2037089450	http://site/
test1.html	131338	2037089450	http://site/
html	196868	2037089450	http://site/
test2	196869	2037089450	http://site/
test2.html	196874	2037089450	http://site/
http	264452	2037089450	http://site/
site	329732	2037089450	http://site/
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	338	1	62841357	http://site/test1.html
200	48	1	1259310415	http://site/test2.html
200	166	0	2037089450	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	62841357	http://site/test1.html	body	 This is the complex body test page.
200	62841357	http://site/test1.html	charset	ISO-8859-1
200	62841357	http://site/test1.html	content-language	en
200	62841357	http://site/test1.html	content-type	text/html
200	62841357	http://site/test1.html	title	Test title
200	1259310415	http://site/test2.html	body	blabla blabla ...
200	1259310415	http://site/test2.html	charset	ISO-8859-1
200	1259310415	http://site/test2.html	content-language	en
200	1259310415	http://site/test2.html	content-type	text/html
200	2037089450	http://site/	body	 test1.html  .  test2.html  .. 
200	2037089450	http://site/	charset	ISO-8859-1
200	2037089450	http://site/	content-language	en
200	2037089450	http://site/	content-type	text/html
200	2037089450	http://site/	title	http://site/
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
