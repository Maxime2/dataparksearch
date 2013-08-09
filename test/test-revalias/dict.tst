FIELDS=OFF;
SELECT dict.word,dict.intag,url.docsize FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.docsize,dict.intag;
