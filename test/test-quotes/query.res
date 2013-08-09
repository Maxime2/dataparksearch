SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	65796	-1419780796	http://site/test1.txt
is	131330	-1419780796	http://site/test1.txt
a	196865	-1419780796	http://site/test1.txt
text	262404	-1419780796	http://site/test1.txt
file	327940	-1419780796	http://site/test1.txt
file.	327941	-1419780796	http://site/test1.txt
no	393474	-1419780796	http://site/test1.txt
title	459013	-1419780796	http://site/test1.txt
available	524553	-1419780796	http://site/test1.txt
available.	524554	-1419780796	http://site/test1.txt
some	590084	-1419780796	http://site/test1.txt
special	655623	-1419780796	http://site/test1.txt
characters	721162	-1419780796	http://site/test1.txt
characters:	721163	-1419780796	http://site/test1.txt
let's	786693	-1419780796	http://site/test1.txt
check	852229	-1419780796	http://site/test1.txt
how	917763	-1419780796	http://site/test1.txt
they	983300	-1419780796	http://site/test1.txt
get	1048835	-1419780796	http://site/test1.txt
escaped	1114375	-1419780796	http://site/test1.txt
escaped.	1114376	-1419780796	http://site/test1.txt
site	66052	1063870766	http://site/
http	66052	1063870766	http://site/
http://site/	66060	1063870766	http://site/
txt	131331	1063870766	http://site/
test1	131333	1063870766	http://site/
test1.txt	131337	1063870766	http://site/
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	149	1	-1419780796	http://site/test1.txt
200	128	0	1063870766	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-1419780796	http://site/test1.txt	body	This is a text file. No title available. Some special characters: &#34; '. ' . ' . ' . ' &#34; . &#34; . &#34; . &#34; \ \ \ \ \ Let's check how they get escaped.
200	-1419780796	http://site/test1.txt	charset	ISO-8859-1
200	-1419780796	http://site/test1.txt	content-language	en
200	-1419780796	http://site/test1.txt	content-type	text/plain
200	1063870766	http://site/	body	 test1.txt  .  .. 
200	1063870766	http://site/	charset	ISO-8859-1
200	1063870766	http://site/	content-language	en
200	1063870766	http://site/	content-type	text/html
200	1063870766	http://site/	title	http://site/
SQL>SQL>
