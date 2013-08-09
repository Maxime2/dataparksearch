SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
SQL>'SELECT status, docsize, hops, crc32, pop_rank, url FROM url ORDER BY status, crc32'
200	718	0	-1889247529	0	http://site/
200	941	1	-1389384011	0.03125	http://site/testpage6.html
200	2989	1	-1216842143	0.03125	http://site/testpage8.html
200	128	1	-696559140	0.03125	http://site/ispattern.html
200	1023	1	-687156161	0.03125	http://site/testpage10.html
200	894	1	-583218275	0.03125	http://site/testpage13.html
200	813	1	-111932412	0.03125	http://site/testpage7.html
200	1592	1	276415977	0.03125	http://site/testpage9.html
200	1041	1	316746425	0.03125	http://site/testpage11.html
200	986	1	885528602	0.03125	http://site/testpage1.html
200	828	1	1197745703	0.03125	http://site/testpage5.html
200	987	1	1228681757	0.03125	http://site/testpage3.html
200	992	1	1688126502	0.03125	http://site/testpage2.html
200	924	1	1718711531	0.03125	http://site/testpage4.html
200	807	1	1813831224	0.03125	http://site/testpage12.html
404	0	0	0	0	http://site/test.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-1889247529	http://site/	body	 testpage4.html  testpage11.html  testpage7.html  testpage2.html  testpage3.html  testpage5.html  ispattern.html  testpage13.html  testpage12.html  testpage8.html  testpage10.html  .  testpage6.html  ..  testpage1.html  testpage9.html 
200	-1889247529	http://site/	charset	UTF-8
200	-1889247529	http://site/	content-language	en
200	-1889247529	http://site/	content-type	text/html
200	-1889247529	http://site/	title	http://site/
200	-1389384011	http://site/testpage6.html	adate	Wed, 24 May 2006 23:06:00 EDT
200	-1389384011	http://site/testpage6.html	body	 The Wall Street Journal Wed, 24 May 2006 23:06:00 EDT machine1
200	-1389384011	http://site/testpage6.html	charset	ISO-8859-1
200	-1389384011	http://site/testpage6.html	content-language	en
200	-1389384011	http://site/testpage6.html	content-type	text/html
200	-1389384011	http://site/testpage6.html	title	TestPage6 title1 title2 title3 title4 title5 title6 title7 title8 title9 title10 title11 title12 title13 title14 title15 title16
200	-1216842143	http://site/testpage8.html	adate	Wed, 24 May 2002 23:06:00 EDT
200	-1216842143	http://site/testpage8.html	body	 The Wall Street Journal Wed, 24 May 2002 23:06:00 EDT word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12 word13 word14 word15 word16 word17 word18 word19 word20 word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 wor
200	-1216842143	http://site/testpage8.html	charset	ISO-8859-1
200	-1216842143	http://site/testpage8.html	content-language	en
200	-1216842143	http://site/testpage8.html	content-type	text/html
200	-1216842143	http://site/testpage8.html	title	TestPage8 300 Word Body
200	-696559140	http://site/ispattern.html	body	 @twitter #trend ++k trend# twitter@gmail.com +1 c++ l'orex
200	-696559140	http://site/ispattern.html	charset	UTF-8
200	-696559140	http://site/ispattern.html	content-language	en
200	-696559140	http://site/ispattern.html	content-type	text/html
200	-696559140	http://site/ispattern.html	title	isPattern
200	-687156161	http://site/testpage10.html	adate	Mon, 24 May 2005 23:06:00 EDT
200	-687156161	http://site/testpage10.html	body	 The Wall Street Journal Mon, 24 May 2005 23:06:00 EDT aaaaaaaaaabbbbbbbbbbcccccccccc aaaaaaaaaabbbbbbbbbbccccccccccdd aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeee aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeee
200	-687156161	http://site/testpage10.html	charset	ISO-8859-1
200	-687156161	http://site/testpage10.html	content-language	en
200	-687156161	http://site/testpage10.html	content-type	text/html
200	-687156161	http://site/testpage10.html	title	TestPage10 Long Words
200	-583218275	http://site/testpage13.html	adate	Tue, 31 Jan 2006 23:06:00 EDT
200	-583218275	http://site/testpage13.html	body	 The Wall Street Journal Tue, 31 Jan 2006 23:06:00 EDT rewrite This is a real-time news story and may be updated in the near future.
200	-583218275	http://site/testpage13.html	charset	ISO-8859-1
200	-583218275	http://site/testpage13.html	content-language	en
200	-583218275	http://site/testpage13.html	content-type	text/html
200	-583218275	http://site/testpage13.html	title	TestPage13 Within 6 months
200	-111932412	http://site/testpage7.html	adate	Wed, 24 May 2006 23:06:00 EDT
200	-111932412	http://site/testpage7.html	body	 The Wall Street Journal Wed, 24 May 2006 23:06:00 EDT title1
200	-111932412	http://site/testpage7.html	charset	ISO-8859-1
200	-111932412	http://site/testpage7.html	content-language	en
200	-111932412	http://site/testpage7.html	content-type	text/html
200	-111932412	http://site/testpage7.html	title	TestPage7 Search Body Content only
200	276415977	http://site/testpage9.html	adate	Mon, 24 May 2005 23:06:00 EDT
200	276415977	http://site/testpage9.html	body	 The Wall Street Journal Mon, 24 May 2005 23:06:00 EDT rewrite
200	276415977	http://site/testpage9.html	charset	ISO-8859-1
200	276415977	http://site/testpage9.html	content-language	en
200	276415977	http://site/testpage9.html	content-type	text/html
200	276415977	http://site/testpage9.html	title	TestPage9 title1 foo2 foo3 foo4 foo5 foo6 foo7 foo8 foo9 foo10 foo11 foo12 foo13 foo14 foo15 foo16 foo17 foo18 foo19 foo20 title
200	316746425	http://site/testpage11.html	adate	Mon, 24 May 2005 23:06:00 EDT
200	316746425	http://site/testpage11.html	body	 The Wall Street Journal Mon, 24 May 2005 23:06:00 EDT amit amit amit aaaaaaaaaabbbbbbbbbbcccccccccc aaaaaaaaaabbbbbbbbbbccccccccccdd aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeee aaaaaaaaaabbbbbbbbbbccccccccccd
200	316746425	http://site/testpage11.html	charset	ISO-8859-1
200	316746425	http://site/testpage11.html	content-language	en
200	316746425	http://site/testpage11.html	content-type	text/html
200	316746425	http://site/testpage11.html	title	TestPage11 Long Words
200	885528602	http://site/testpage1.html	adate	Thu, 25 May 2006 23:06:00 EDT
200	885528602	http://site/testpage1.html	body	 The Wall Street Journal Thu, 25 May 2006 23:06:00 EDT BODY1 BODY2 BODY3 BODY4 BODY5 BODY6 BODY7 BODY8 BODY9 BODY10 BODY11 BODY12 BODY13 BODY14 BODY15 BODY16 BODY17 BODY18 BODY19 BODY20 BODY21 BODY22 BODY23 BODY24 BODY25 BODY26 BODY27 BODY28 BODY29 BODY30
200	885528602	http://site/testpage1.html	charset	ISO-8859-1
200	885528602	http://site/testpage1.html	content-language	en
200	885528602	http://site/testpage1.html	content-type	text/html
200	885528602	http://site/testpage1.html	title	TestPage1
200	1197745703	http://site/testpage5.html	adate	Mon, 24 May 2006 23:06:00 EDT
200	1197745703	http://site/testpage5.html	body	 The Wall Street Journal Mon, 24 May 2006 23:06:00 EDT American typo Insurance Group
200	1197745703	http://site/testpage5.html	charset	ISO-8859-1
200	1197745703	http://site/testpage5.html	content-language	en
200	1197745703	http://site/testpage5.html	content-type	text/html
200	1197745703	http://site/testpage5.html	title	TestPage5 Phrase searching
200	1228681757	http://site/testpage3.html	adate	Thu, 26 May 2006 23:06:00 EDT
200	1228681757	http://site/testpage3.html	body	 The Wall Street Journal Thu, 26 May 2006 23:06:00 EDT BODY2 BODY4 BODY6 BODY8 BODY10 BODY12 BODY14 BODY16 BODY18 BODY20 BODY22 BODY24 BODY26 BODY28 BODY30 BODY31 BODY32 BODY33 BODY34 BODY35 BODY36 BODY37 BODY38 BODY39 BODY40
200	1228681757	http://site/testpage3.html	charset	ISO-8859-1
200	1228681757	http://site/testpage3.html	content-language	en
200	1228681757	http://site/testpage3.html	content-type	text/html
200	1228681757	http://site/testpage3.html	title	TestPage3 Even Number of BODY's until 30
200	1688126502	http://site/testpage2.html	adate	Thu, 25 May 2006 23:06:00 EDT
200	1688126502	http://site/testpage2.html	body	 The Wall Street Journal Thu, 25 May 2006 23:06:00 EDT BODY1 BODY3 BODY5 BODY7 BODY9 BODY11 BODY13 BODY15 BODY17 BODY19 BODY21 BODY23 BODY25 BODY27 BODY29 BODY30 BODY31 BODY32 BODY33 BODY34 BODY35 BODY36 BODY37 BODY38 BODY39 BODY40
200	1688126502	http://site/testpage2.html	charset	ISO-8859-1
200	1688126502	http://site/testpage2.html	content-language	en
200	1688126502	http://site/testpage2.html	content-type	text/html
200	1688126502	http://site/testpage2.html	title	TestPage2 Odd Number of BODY's until 30
200	1718711531	http://site/testpage4.html	adate	Mon, 25 May 2006 23:06:00 EDT
200	1718711531	http://site/testpage4.html	body	 The Wall Street Journal Mon, 25 May 2006 23:06:00 EDT The government has a million dollar system. The policy comes from the American Insurance Group, or AIG.
200	1718711531	http://site/testpage4.html	charset	ISO-8859-1
200	1718711531	http://site/testpage4.html	content-language	en
200	1718711531	http://site/testpage4.html	content-type	text/html
200	1718711531	http://site/testpage4.html	title	Testpage4 Stopwords and Phrase Search Check
200	1813831224	http://site/testpage12.html	adate	Sun, 30 Apr 2006 23:06:00 EDT
200	1813831224	http://site/testpage12.html	body	 The Wall Street Journal Sun, 30 Apr 2006 23:06:00 EDT rewrite
200	1813831224	http://site/testpage12.html	charset	ISO-8859-1
200	1813831224	http://site/testpage12.html	content-language	en
200	1813831224	http://site/testpage12.html	content-type	text/html
200	1813831224	http://site/testpage12.html	title	TestPage12 Within Three Months
SQL>'SELECT u1.docsize,u2.docsize,u1.url,u2.url FROM url u1,url u2, links l WHERE u1.rec_id=l.ot AND u2.rec_id=l.k ORDER BY u1.docsize,u2.docsize'
718	128	http://site/	http://site/ispattern.html
718	718	http://site/	http://site/
718	807	http://site/	http://site/testpage12.html
718	813	http://site/	http://site/testpage7.html
718	828	http://site/	http://site/testpage5.html
718	894	http://site/	http://site/testpage13.html
718	924	http://site/	http://site/testpage4.html
718	941	http://site/	http://site/testpage6.html
718	986	http://site/	http://site/testpage1.html
718	987	http://site/	http://site/testpage3.html
718	992	http://site/	http://site/testpage2.html
718	1023	http://site/	http://site/testpage10.html
718	1041	http://site/	http://site/testpage11.html
718	1592	http://site/	http://site/testpage9.html
718	2989	http://site/	http://site/testpage8.html
SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
