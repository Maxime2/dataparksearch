<!--variables
DBAddr		searchd://v.sochi.org.ru/
DBAddr		searchd://v.sochi.org.ru/?label=sochi
DBAddr		searchd://v.sochi.org.ru/?label=phones
DBAddr		searchd://v.sochi.org.ru/?label=qsimilar
StoredocURL     /cgi-bin/storedoc.cgi
LocalCharset 	koi8-r
BrowserCharset  UTF-8
DoExcerpt no

MinWordLength 1
MaxWordLength 64
DetectClones no
DateFormat %d %b %Y
ExcerptSize 128
ExcerptPadding 32
HlBeg <span class="rhl">
HlEnd </span>
GrBeg <!-- -->
GrEnd <!-- -->
Locale ru_RU.UTF-8
-->

<!--top-->
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
   "http://www.w3.org/TR/html4/loose.dtd">
<html lang="ru">
  <head>
    <title>$&(q)</title>
</head>
<!-- BrowserCharset: $(BrowserCharset) --><!IF NAME="s_tmplt" CONTENT=""><!SET NAME="s_tmplt" CONTENT="janus.htm.ru"><!ENDIF>
<body id="Content"> 
<!--/top-->

<!--bottom-->
</body>
</html>
<!--/bottom-->

<!--restop-->
<div id="topmenu"><b>Похожие запросы:</b></div>
<table border="0" cellspacing="0" cellpadding="0"><tr><td align="left">
<!--/restop-->


<!--res-->
<!IFNOT NAME="query" CONTENT="">
<div class="qsimilar">
<small class="restop"><!IF NAME="s_cp" CONTENT="koi8-r"><a class="restop" href="$(self)?q=$%(query:koi8-r)&amp;m=$&(mode)&amp;c=$&(s_c)&amp;sp=$&(s_sp)&amp;sy=$&(s_sy)&amp;GroupBySite=$&(s_GroupBySite)&amp;tmplt=$&(s_tmplt)&amp;s=IRPD&amp;label=$&(s_label)">
<!ELSE><a class="restop" href="$(self)?q=$%(query)&amp;m=$&(mode)&amp;c=$&(s_c)&amp;sp=$&(s_sp)&amp;sy=$&(s_sy)&amp;GroupBySite=$&(s_GroupBySite)&amp;tmplt=$&(s_tmplt)&amp;s=IRPD&amp;label=$&(s_label)"><!ENDIF>
$&(query:128)</a></small><br/>
</div>
<!ENDIF>
<!--/res-->

<!--clone-->
<li><A HREF="$(URL)">$(URL:50)</A>,&nbsp; $(Last-Modified)
<!--/clone-->

<!--navleft-->
<TD><A class="leftmenu" HREF="$(self)$(NH)"><small>&laquo;&laquo;Пред.&laquo;</small></A></TD>
<!--/navleft-->

<!--navbar0-->
<TD><b class="hl">$(NP)</b></TD>
<!--/navbar0-->

<!--navbar1-->
<TD><A class="leftmenu" HREF="$(self)$(NH)">$(NP)</A></TD>
<!--/navbar1-->

<!--navright-->
<TD><A class="leftmenu" HREF="$(self)$(NH)"><small>&raquo;След.&raquo;&raquo;</small></A></TD>
<!--/navright-->

<!--navigator-->
<table border="0" cellspacing="0" cellpadding="3"><tr><td><small>Страницы:</small></td>$(NL) $(NB) $(NR)</tr></table>
<!--/navigator-->

<!--resbot-->
</td></tr></table>
<br/>
<!--/resbot-->


<!--notfound-->
&nbsp;
<!--/notfound-->

<!--noquery-->
&nbsp;
<!--/noquery-->

<!--error-->
<CENTER>
<FONT COLOR="#FF0000">Ошибка!</FONT>
<P>
<B>$(E)</B>
</CENTER>
<!--/error-->


