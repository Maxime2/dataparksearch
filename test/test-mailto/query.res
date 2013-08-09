SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
report	65798	-1951370482	http://site/test.html
bugs	131332	-1951370482	http://site/test.html
here	196868	-1951370482	http://site/test.html
http	66052	981599134	http://site/
site	66052	981599134	http://site/
http://site/	66060	981599134	http://site/
html	131332	981599134	http://site/
test	131332	981599134	http://site/
test.html	131337	981599134	http://site/
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	100	1	-1951370482	http://site/test.html
200	128	0	981599134	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-1951370482	http://site/test.html	body	 Report bugs here 
200	-1951370482	http://site/test.html	charset	ISO-8859-1
200	-1951370482	http://site/test.html	content-language	en
200	-1951370482	http://site/test.html	content-type	text/html
200	981599134	http://site/	body	 .  test.html  .. 
200	981599134	http://site/	charset	ISO-8859-1
200	981599134	http://site/	content-language	en
200	981599134	http://site/	content-type	text/html
200	981599134	http://site/	title	http://site/
SQL>SQL>
