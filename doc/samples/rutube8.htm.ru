<!--variables
DBAddr		searchd://localhost/?label=rutube
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
<!-- BrowserCharset: $(BrowserCharset) -->
<body id="Content"> 
<!--/top-->

<!--bottom-->
</body>
</html>
<!--/bottom-->

<!--restop-->
<div id="topmenu"><b>Видеоролики:</b></div>
<table border="0" cellspacing="0" cellpadding="0"><tr><td align="left">
<!--/restop-->


<!--res-->
<div class="serp1">
<small class="restop"><a class="restop" href="$(url)">$&(title:128)</a></small><br>
<small class="restop"><a class="restop" href="$(url)">
<!IFNOT NAME="ytid" VALUE=""><img src="http://img.youtube.com/vi/$(ytid)/default.jpg" border="0"></a> 
<!ELSEIFNOT NAME="myvi" VALUE=""><!IF NAME="ruid" VALUE=""><!SET NAME="ruid" CONTENT="a"><!ENDIF><img src="http://fs-$*(ruid).myvi.ru/$*(myvi).th1" border="0"></a>
<!ELSE><img src="http://img.rutube.ru/thumbs/$&(ruid)-2.jpg" border="0"></a><!ENDIF>
</small><br>
<!IFNOT NAME="meta.description" CONTENT=""><div class="serp2"><small>$&(meta.description:128)</small></div>
<!ELSE>
 <!IFNOT NAME="meta.keywords" CONTENT=""><div class="serp2"><small>$&(meta.keywords:128)</small></div><!ENDIF>
<!ENDIF>
<small class="result">$(Last-Modified) - $(Score)</small>
</div>
<!--/res-->

<!--clone-->
<li><A HREF="$(URL)">$(URL:50)</A>, $(Last-Modified)
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
<div class="bottom" align="center">
<a href="/cgi-bin/search.cgi?q=$%(q:utf-8)&m=$&(m)&g=$&(g)&sp=$&(sp)&sy=$&(sy)&GroupBySite=no&s=RD&tmplt=vi8.htm.ru&label=rutube"><small>больше видео</small></a>
</div>
<br>
<!--/resbot-->


<!--notfound-->

<!--/notfound-->

<!--noquery-->

<!--/noquery-->

<!--error-->
<CENTER>
<FONT COLOR="#FF0000">Ошибка!</FONT>
<P>
<B>$(E)</B>
</CENTER>
<!--/error-->


