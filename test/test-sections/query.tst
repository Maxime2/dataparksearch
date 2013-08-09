FIELDS=OFF;
SELECT dict.word,dict.intag,url.crc32,last_mod_time,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag,dict.word;
SELECT * FROM crossdict ORDER BY url_id,intag;
SELECT dict.word,dict.intag,url.crc32,url.url,ref.url FROM crossdict dict, url, url ref WHERE url.rec_id=dict.url_id AND ref.rec_id=dict.ref_id ORDER BY url.crc32,dict.intag;

SELECT dict.word,dict.intag,url.crc32,dict.url_id,dict.ref_id FROM crossdict dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag;

#SELECT dict.word_id,dict.intag,url.crc32,url.url,ref.url FROM ncrossdict dict, url, url ref WHERE url.rec_id=dict.url_id AND ref.rec_id=dict.ref_id ORDER BY url.crc32,dict.itag;
SELECT status, docsize, hops, crc32, last_mod_time, url FROM url ORDER BY status, crc32;
SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname);

SELECT url FROM url WHERE url='http://site/';
