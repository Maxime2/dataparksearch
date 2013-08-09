SQL>'FIELDS=OFF'
SQL>'SELECT url FROM url WHERE url LIKE '%1.php%''
http://site/1.php
SQL>'SELECT url FROM url WHERE url LIKE '%2l.php%''
http://site/2l.php?a=b
SQL>'SELECT url FROM url WHERE url LIKE '%2r.php%''
http://site/2r.php?a=b
SQL>'SELECT url FROM url WHERE url LIKE '%2m.php%''
http://site/2m.php?a=b&c=d
SQL>'SELECT url FROM url WHERE url LIKE '%3l.php%''
http://site/3l.php?a=b&c=d
SQL>'SELECT url FROM url WHERE url LIKE '%3r.php%''
http://site/3r.php?a=b&c=d
SQL>'SELECT url FROM url WHERE url LIKE '%3m.php%''
http://site/3m.php?a=b&c=d
SQL>'SELECT url FROM url WHERE url LIKE '%sid=%''
SQL>
