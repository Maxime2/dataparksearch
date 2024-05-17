<!--variables
DBAddr		searchd://localhost/
DBAddr		searchd://localhost/?label=sochi
DBAddr  searchd://localhost/?label=phones
DBAddr  searchd://localhost/?label=qsimilar
DBAddr  searchd://localhost/?label=rutube
DBAddr  searchd://localhost/?label=tabu
DBAddr  searchd://localhost/?label=momentum
StoredocURL     /cgi-bin/storedoc.cgi
LocalCharset 	utf-8
BrowserCharset  UTF-8
Locale ru_RU.UTF-8
HTTPHeader "Cache-control: public"

#Alias http://www.sochi.ru/ http://www.sochi.com/

#Include i18n/ru.koi8-r.lang
ReplaceVar 43N39E.lng "43&deg;с.ш. 39&deg;в.д."
ReplaceVar all.lng "все слова"
ReplaceVar any.lng "любое из слов"
ReplaceVar AU.lng "Поиск по Австралии"
ReplaceVar BE.lng "Поиск по Бельгии"
ReplaceVar Books.lng "Книги на 43&deg;с.ш. 39&deg;в.д."
ReplaceVar Compr.lng "Поиск по серверам, посвященным сжатию данных"
ReplaceVar Lib.lng "Поиск по библиотеке Мошкова"
ReplaceVar Justine.lng "Страницы Интернет о Жюстин Энен-Арден"
ReplaceVar Job.lng "Работа в Сочи"
ReplaceVar Ads.lng "Объявления в Сочи"
ReplaceVar Dpsearch.lng "Сайт DataparkSearch"
ReplaceVar Sochived.lng "Сочинский краевед"
ReplaceVar Momentum.lng "Поиск по страницам Издательства Моментум"
ReplaceVar sochinews.lng "Новости"
ReplaceVar near.lng "все слова рядом"
ReplaceVar NZ.lng "Поиск по Новой Зеландии"
ReplaceVar Oregon.lng "Поиск по университету Орегона"
ReplaceVar Search.lng "Поиск"
ReplaceVar SearchFor.lng "Искать"
ReplaceVar bool.lng "логический запрос"
ReplaceVar error.lng "Ошибка!"
ReplaceVar noquery.lng "Вы не задали ни одного слова для поиска."
ReplaceVar notfound.lng "По данному запросу ничего не найдено."
ReplaceVar try.lng "Попробуйте упростить запрос или проверьте правописание."
ReplaceVar langany.lng "любой язык"
ReplaceVar langru.lng "русский язык"
ReplaceVar langen.lng "английский язык"
ReplaceVar langde.lng "немецкий язык"
ReplaceVar langfr.lng "французский язык"
ReplaceVar langzh.lng "китайский язык"
ReplaceVar langko.lng "корейский язык"
ReplaceVar langth.lng "тайский язык"
ReplaceVar langja.lng "японский язык"
ReplaceVar gr.lng "группировать по сайтам"
ReplaceVar nogr.lng "не группировать"
ReplaceVar syn.lng "с синонимами"
ReplaceVar nosyn.lng "без синонимов"
ReplaceVar doc.lng "во всём документе"
ReplaceVar ref.lng "в реферате"
ReplaceVar tit.lng "в заголовке"
ReplaceVar body.lng "в теле"
ReplaceVar mp3.lng "в тэгах MP3"
ReplaceVar alldocs.lng "все документы"
ReplaceVar lastweek.lng "за последнюю неделю"
ReplaceVar last2w.lng "за последние две недели"
ReplaceVar lastmonth.lng "за последний месяц"
ReplaceVar last3m.lng "за последние 3 месяца"
ReplaceVar last6m.lng "за последние 6 месяцев"
ReplaceVar lastyear.lng "за последний год"
ReplaceVar exact.lng "точные слова"
ReplaceVar wforms.lng "варианты слов"
ReplaceVar otherse.lng "в других поисковых системах"
ReplaceVar yandex.lng "Яндекс"
ReplaceVar rambler.lng "Рамблер"
ReplaceVar nigma.lng "Нигма"
ReplaceVar datapark.lng "ДатаПарк"
ReplaceVar usingdp.lng "Используя DataparkSearch"
ReplaceVar showndocs.lng "Показаны документы"
ReplaceVar iz.lng "из"
ReplaceVar found.lng "найденных"
ReplaceVar stime.lng "Время поиска"
ReplaceVar sec.lng "сек."
ReplaceVar results.lng "Результаты поиска"
ReplaceVar doyoumean.lng "Имеется в виду"
ReplaceVar pages.lng "Страницы"
ReplaceVar rssres.lng "RSS-поток самых свежих результатов по запросу"
ReplaceVar sortedby.lng "отсортировано по"
ReplaceVar isort.lng "важности"
ReplaceVar rsort.lng "соответствию"
ReplaceVar dsort.lng "дате"
ReplaceVar psort.lng "популярности"
ReplaceVar notitle.lng "[без заголовка]"
ReplaceVar artist.lng "Исполнитель"
ReplaceVar album.lng "Альбом"
ReplaceVar song.lng "Композиция"
ReplaceVar year.lng "Год"
ReplaceVar summary.lng "Реферат"
ReplaceVar stcopy.lng "копия"
ReplaceVar allresfrom.lng "Все результаты с сайта"
ReplaceVar more.lng "ещё"
ReplaceVar onmap.lng "на карте"
ReplaceVar price.lng "Цена"
ReplaceVar phone.lng "Тел."
ReplaceVar rur.lng "руб."
ReplaceVar prev.lng "Пред."
ReplaceVar next.lng "След."
ReplaceVar instplugin.lng "Установить поисковый плагин"
ReplaceVar mkreport.lng "Написать отзыв"
ReplaceVar bytes.lng "байт"
ReplaceVar score.lng "Рейтинг"
ReplaceVar ToFind.lng "Найти"
ReplaceVar extended.lng "Расширеный поиск"
ReplaceVar Sochi.org.ru.lng "Интернет Сочи"
ReplaceVar wholinks.lng "Ссылки"
ReplaceVar qwords.lng "Слова запроса"
ReplaceVar sochi24.lng "Сочи-24"
ReplaceVar translate.lng "перевод"
ReplaceVar lastmod.lng "На дату:"
ReplaceVar release.lng "Дата выхода"

<!IF NAME="c" CONTENT="01">
<!IF NAME="label" CONTENT="">
<!SET NAME="label" CONTENT="sochi">
<!ENDIF>
<!ENDIF>
<!IF NAME="label" CONTENT="momentum">
<!SET NAME="MaxSiteLevel" CONTENT="-1">
<!ENDIF>

MinWordLength 1
MaxWordLength 64
DetectClones no
ExcerptSize 240
ExcerptPadding 72
HlBeg <span class="rhl">
HlEnd </span>
GrBeg <div class="group">
GrEnd </div>
ExcerptMark &nbsp;...<br>
DateFormat %d.%m.%Y
m all
-->

<!--top-->
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<HTML lang="ru"><!IF NAME="c" CONTENT="01"><!IF NAME="label" CONTENT=""><!SET NAME="label" CONTENT="sochi"><!ENDIF><!ENDIF>
  <HEAD>
    <TITLE>$&(q)<!IF NAME="label" CONTENT="sochi"> — $(Sochi.org.ru.lng)<!ENDIF>, страниц: $(grand_total)</TITLE>
    <META HTTP-EQUIV="Content-Type" Content="text/html; charset=$(BrowserCharset)">
    <link href="/favicon.ico" rel="shortcut icon" type="image/x-icon">
    <link rel="alternate" type="application/rss+xml" title="RSS 2.0" href="$(self)?q=$%(q)&amp;c=$&(c)&amp;site=$&(site)&amp;m=$&(m)&amp;sp=$&(sp)&amp;sy=$&(sy)&amp;&amp;s=$&(s)&amp;label=$&(label)&amp;tmplt=rss8.htm.ru">
    <link title="$(43N39E.lng)" type="application/opensearchdescription+xml" rel="search" href="/opensearch-en.xml">
	<style type="text/css">
		BODY { margin-left: 1em; margin-right: 1em; }
		SELECT { width: 15em; }
	</style>
    <link href="/css/therm2.css" rel="stylesheet" type="text/css">
 </HEAD>
<!-- BrowserCharset: $(BrowserCharset) -->
<!-- $* of q: $*(q) -->
<body>

<div id="form" style="max-width: 70em;">
<center>
<table width="100%" border="0" cellpadding="0" cellspacing="0" align="center" style="background: url(/tdbg2.png) repeat-x #f4f4f4;">

<tr>
<td class="headruler" align="left" valign="top">&nbsp;<b>
<!IF NAME="c" CONTENT="0B09"><a href="http://books.43n39e.ru/">$(Books.lng)</a>
<!ELIF NAME="c" CONTENT="0101"><a href="http://inet-sochi.ru/job/">$(Job.lng)</a>
<!ELIF NAME="c" CONTENT="0102"><a href="http://inet-sochi.ru/ads/">$(Ads.lng)</a>
<!ELIF NAME="c" CONTENT="0B0A"><a href="http://notes.sochi.org.ru/3761">$(Lib.lng)</a>
<!ELIF NAME="c" CONTENT="010N"><a href="http://sochived.info/">$(Sochived.lng)</a>
<!ELIF NAME="c" CONTENT="010S"><a href="http://inet-sochi.ru/news/">$(sochinews.lng)</a>
<!ELIF NAME="site" CONTENT="-572280698"><a href="http://www.dataparksearch.org/">$(Dpsearch.lng)</a>
<!ELIF NAME="site" CONTENT="-1483588461"><a href="http://www.dataparksearch.org/">$(Dpsearch.lng)</a>
<!ELIF NAME="label" CONTENT="sochi"><a href="http://inet-sochi.ru/">$(Sochi.org.ru.lng)</a>
<!ELIF NAME="label" CONTENT="momentum"><a href="http://inet-sochi.ru/momentum">$(Momentum.lng)</a>
<!ELIF NAME="c" CONTENT="09"><a href="http://inet-sochi.ru/gday">$(AU.lng)</a>
<!ELIF NAME="c" CONTENT="0F"><a href="http://www.43n39e.ru/nz">$(NZ.lng)</a>
<!ELIF NAME="c" CONTENT="0K"><a href="http://www.43n39e.ru/be">$(BE.lng)</a>
<!ELIF NAME="c" CONTENT="0D"><a href="http://www.43n39e.ru/oregon">$(Oregon.lng)</a>
<!ELIF NAME="c" CONTENT="04"><a href="http://www.maxime.net.ru/">$(Compr.lng)</a>
<!ELIF NAME="c" CONTENT="06"><a href="http://justine.43n39e.ru/">$(Justine.lng)</a>
<!ELSE><a href="http://www.43n39e.ru/">$(43N39E.lng)</a>
<!ENDIF>
</b></td>
</tr>

<tr>
<td valign="middle" align="center">

	    <form id="fo0" method="GET" action="$(self)" enctype="application/x-www-form-urlencoded">
	      <input type="hidden" name="ps" value="10">
	      <input type="hidden" name="np" value="0">
	      <input type="hidden" name="dt" value="back">
<!IFNOT NAME="t" CONTENT=""><input type="hidden" name="t" value="$&(t)"><!ENDIF>
<!IFNOT NAME="c" CONTENT=""><input type="hidden" name="c" value="$&(c)"><!ENDIF>
	      <input type="hidden" name="tmplt" value="janus.htm.ru">
	      <input type="hidden" name="label" value="$&(label)">
	      <input type="hidden" name="s" value="$&(s)">
<div id="exth">
	<input type="hidden" name="m" value="$&(m)">
	<input type="hidden" name="g" value="$&(g)">
	<input type="hidden" name="GroupBySite" value="$&(GroupBySite)">
	<input type="hidden" name="sy" value="$&(sy)">
	<input type="hidden" name="rm" value="$&(rm)">
<!IFNOT NAME="wf" CONTENT=""><input type="hidden" name="wf" value="$&(wf)"><!ENDIF>
	<input type="hidden" name="dp" value="$&(dp)">
	<input type="hidden" name="sp" value="$&(sp)">
</div>
<!IF NAME="site" CONTENT="-572280698">
	<input type="hidden" name="site" value="-572280698">
<!ENDIF>
<!IF NAME="site" CONTENT="-1483588461">
	<input type="hidden" name="site" value="-1483588461">
<!ENDIF>

<table width="100%" border="0" cellpadding="2" cellspacing="0">
<tbody id="ext0">
<tr>
<td width="100%">
			<table width="100%" border="0" cellpadding="2" cellspacing="0">
			    <tr>
			      <td width="99%" style="vertical-align: middle;" align="left">
				<div id="demo" class="yui3-skin-sam">
				  <input class="inputsearch" type="text" name="q" id="q" size="60" value="$&(q)" style="width:100%;" autocomplete="off">
				</div>
			      </td><td>
				  <input class="inputsearch" type="submit" value="$(ToFind.lng)">
			      </td>
			    </tr>
			    <!--tr>
			      <td class="inputrev">&nbsp;</td>
			    </tr-->
			</table>
</td>
</tr>
<tr><td width="100%" align="right"><small><a href="#" onMouseDown="return swap();">$(extended.lng)</a></small></td></tr>
<tr id="ext1" style="visibility: hidden;">
<td width="100%" align="right">
 <select name="m" class="inputrev">
 <option value="all" selected="$&(m)"> $(all.lng)
 <option value="near" selected="$&(m)"> $(near.lng)
 <option value="any" selected="$&(m)"> $(any.lng)
 <option value="bool" selected="$&(m)"> $(bool.lng)
 </select>
<select class="inputrev" name="g">
<OPTION VALUE="" SELECTED="$&(g)">$(langany.lng)</option>
<OPTION VALUE="ru" SELECTED="$&(g)">$(langru.lng)</option>
<OPTION VALUE="en" SELECTED="$&(g)">$(langen.lng)</option>
<OPTION VALUE="de" SELECTED="$&(g)">$(langde.lng)</option>
<OPTION VALUE="fr" SELECTED="$&(g)">$(langfr.lng)</option>
<OPTION VALUE="zh" SELECTED="$&(g)">$(langzh.lng)</option>
<OPTION VALUE="ko" SELECTED="$&(g)">$(langko.lng)</option>
<OPTION VALUE="th" SELECTED="$&(g)">$(langth.lng)</option>
<OPTION VALUE="ja" SELECTED="$&(g)">$(langja.lng)</option>
</select>
<select class="inputrev" name="GroupBySite">
<option value="yes" selected="$&(GroupBySite)">$(gr.lng)</option>
<option value="no"  selected="$&(GroupBySite)">$(nogr.lng)</option>
</select>
<select class="inputrev" name="sy">
<option value="0" selected="$&(sy)">$(nosyn.lng)</option>
<option value="1" selected="$&(sy)">$(syn.lng)</option>
</select><br>
</td></tr>

<tr id="ext2" style="visibility: hidden;">
<td width="100%" align="right">
<select class="inputrev" name="rm">
<option value="2" selected="$&(rm)">full</option>
<option value="1" selected="$&(rm)">fast</option>
<option value="3" selected="$&(rm)">ultra</option>
</select>
<!IFNOT NAME="wf" CONTENT="">
<select class="inputrev" name="wf">
<option value="F1F1" selected="$&(wf)">$(doc.lng)</option>
<option value="00000F00" selected="$&(wf)">$(ref.lng)</option>
<option value="000000F0" selected="$&(wf)">$(tit.lng)</option>
<option value="0000000F" selected="$&(wf)">$(body.lng)</option>
</select>
<!ENDIF>
<SELECT class="inputrev" NAME="dp">
<OPTION VALUE="0" SELECTED="$&(dp)">$(alldocs.lng)</option>
<OPTION VALUE="7d" SELECTED="$&(dp)">$(lastweek.lng)</option>
<OPTION VALUE="14d" SELECTED="$&(dp)">$(last2w.lng)</option>
<OPTION VALUE="1m" SELECTED="$&(dp)">$(lastmonth.lng)</option>
<OPTION VALUE="3m" SELECTED="$&(dp)">$(last3m.lng)</option>
<OPTION VALUE="6m" SELECTED="$&(dp)">$(last6m.lng)</option>
<OPTION VALUE="1y" SELECTED="$&(dp)">$(lastyear.lng)</option>
</select>
<select class="inputrev" name="sp">
<option value="0" selected="$&(sp)">$(exact.lng)</option>
<option value="1" selected="$&(sp)">$(wforms.lng)</option>
</select>
</td></tr>
</tbody>
</table>
</FORM>
</td>
</tr>
</table>
</center>
</div>
<!--/top-->

<!--bottom-->
<div id="bottom">
<br>
<!IFNOT NAME="q" CONTENT="">
&nbsp;<small>$(SearchFor.lng) &laquo;$&(q)&raquo; $(otherse.lng):
<a href="http://yandex.ru/yandsearch?text=$%(q:utf-8)&amp;lr=239">$(yandex.lng)</a> &#8211;
<a href="http://www.google.com/search?ie=UTF-8&amp;hl=ru&amp;q=$%(q:utf-8)">Google</a> &#8211;
<a href="http://go.mail.ru/search?q=$%(q:cp1251)">Mail.ru</a> &#8211;
<a href="http://www.rambler.ru/srch?oe=1251&amp;set=www&amp;words=$%(q:cp1251)">$(rambler.lng)</a> &#8211;
<a href="http://www.bing.com/search?q=$%(q:UTF-8)">Bing</a> &#8211;
<a href="http://www.nigma.ru/index.php?0=1&amp;1=1&amp;2=1&amp;3=1&amp;4=1&amp;5=1&amp;6=1&amp;7=1&amp;q=$%(q:cp1251)">$(nigma.lng)</a> &#8211;
<a href="http://search.yahoo.com/bin/query?p=$%(q:UTF-8)&amp;ei=UTF-8">Yahoo!</a>
</small>
<!ENDIF>
<table border="0" width="100%">
<tr>
<td align="center" valign="top">
	&nbsp;
</td>
</tr>
<tr><td align="center" valign="middle">
<small>&copy;2013, &laquo;<a href="http://www.datapark.ru/">$(datapark.lng)</a>&raquo;<br>
<a href="http://www.dataparksearch.org/">$(usingdp.lng)</a></small><br>
</td></tr>
</table>
</div>
<script src="http://yui.yahooapis.com/3.11.0/build/yui/yui-min.js"></script>
    <script type="text/javascript">
    YUI().use('autocomplete', 'autocomplete-filters', 'autocomplete-highlighters', 'node-load', function (Y) {
<!IF NAME="label" CONTENT="sochi">
    Y.one('#similarArea').load('/cgi-bin/search.cgi?q=$%(q:utf-8)&s_c=$&(c)&s_sp=$&(sp)&s_sy=$&(sy)&m=any&sp=1&sy=1&p=$&(p)&GroupBySite=no&s=$&(s)&s_GroupBySite=$&(GroupBySite)&ps=5&tmplt=qsimilar8.htm.ru&label=qsimilar&s_label=$&(label)&mode=$(m)');
    Y.one('#rutubeArea').load('/cgi-bin/search.cgi?q=$%(q:utf-8)&m=$&(m)&g=$&(g)&sp=$&(sp)&sy=$&(sy)&p=$&(p)&GroupBySite=no&s=$&(s)&ps=2&tmplt=rutube8.htm.ru&label=rutube');
    Y.one('#resultArea').load('/cgi-bin/search.cgi?q=$%(q:utf-8)&m=$&(m)&g=$&(g)&sp=1&sy=1&p=$&(p)&GroupBySite=no&s=$&(s)&link=$&(link)&ps=4&tmplt=phones8.htm.ru&label=phones');
<!ELSE>
    Y.one('#similarArea').load('/cgi-bin/search.cgi?q=$%(q:utf-8)&s_c=$&(c)&s_sp=$&(sp)&s_sy=$&(sy)&m=any&sp=1&sy=1&p=$&(p)&GroupBySite=no&s=$&(s)&s_GroupBySite=$&(GroupBySite)&ps=7&tmplt=qsimilar8.htm.ru&label=qsimilar');
    Y.one('#resultArea').load('/cgi-bin/search.cgi?q=$%(q:utf-8)&m=$&(m)&g=$&(g)&sp=$&(sp)&sy=$&(sy)&p=$&(p)&GroupBySite=no&s=$&(s)&ps=4&tmplt=rutube8.htm.ru&label=rutube');
<!ENDIF>
    Y.one('#q').plug(Y.Plugin.AutoComplete, {
    resultFilters    : 'phraseMatch',
    resultHighlighter: 'phraseMatch',
<!IF NAME="label" CONTENT="momentum">
    source           : '/cgi-bin/search.cgi?q={query}&m=any&sp=1&sy=0&GroupBySite=yes&s=IRPD&&ps=10&tmplt=yui.htm&label=qsimilar&callback={callback}'
<!ELSE>
    source           : '/cgi-bin/search.cgi?q={query}&m=any&sp=1&sy=0&GroupBySite=yes&s=IRPD&&ps=10&tmplt=yui.htm&label=qsimilar&callback={callback}'
<!ENDIF>
    });
  });
</script>
    <script type="text/javascript">
      var dpstate = 'h';
      var ext1Content = null;
      var ext2Content = null;
      var exthContent = null;
      var mainContent = null;
      var formContent = null;
      function dp_init() {
	mainContent = document.getElementById('ext0');
	formContent = document.getElementById('fo0');
	ext1Content = document.getElementById('ext1');
	ext2Content = document.getElementById('ext2');
	exthContent = document.getElementById('exth');
	try {
		mainContent.removeChild(ext1Content);
		mainContent.removeChild(ext2Content);
	} catch (ex) {
	}
	ext1Content.style.visibility = 'visible';
	ext2Content.style.visibility = 'visible';
      }
      function swap() {
	if (dpstate == 'h') {
		mainContent.appendChild(ext1Content);
		mainContent.appendChild(ext2Content);
		formContent.removeChild(exthContent);
		dpstate = 'v';
	} else {
	        mainContent.removeChild(ext1Content);
        	mainContent.removeChild(ext2Content);
		formContent.appendChild(exthContent);
		dpstate = 'h';
	}
	return false;
      }
      function clk(id,pos){
	var u = new Date().getTime();
        var i = new Image();
	i.src="/cgi-bin/c.pl?id="+id+"&pos="+pos+'&u='+u;return true;
      }
      dp_init();
</script>
</body>
</html>
<!--/bottom-->

<!--restop-->
<!-- WE: $(WE) -->
<!-- WA: $(WA) -->
<!-- WS: $(WS) -->
<!-- W: $(W) -->

<div class="news" style="max-width: 70em;">
<table width="100%" border="0" cellspacing="0" cellpadding="0">
<tr><td colspan="2" align="left" class="serp">
<small>&nbsp;$(showndocs.lng) <b>$(first)</b>-<b>$(last)</b> $(iz.lng) <b>$(grand_total)</b> $(found.lng).</small>
<td>
<td width="50%" align="right">
<small>$(stime.lng):&nbsp;<b>$(SearchTime)</b> $(sec.lng)&nbsp;</small>
</td></tr>
</table>
</div>
<div class="news" style="max-width: 70em;">
<!IFNOT NAME="Suggest_q" VALUE=""><small>&nbsp;<b>$(doyoumean.lng) &laquo;<a class="newcat" href="$(Suggest_url)">$(Suggest_q)</a>&raquo; ?</b></small>
<!ENDIF>
</div>
<div id="dir" style="max-width: 80em;">
<table cellpadding="5" cellspacing="0" border="0">
<tr>
<td rowspan="2" valign="top" align="left" style="padding-right: 15px;">
<!-- a -->
<!--/restop-->


<!--res-->
<!-- m -->
<div style="max-width: 42em; min-width: 25em;">
<div class="serp1"><i class="serp1" style="background-image:url(http://www.google.com/s2/favicons?domain=$(url.host))"></i>
<span style="margin-left:20px;">&#8203;</span>
$(Order).&nbsp;<!-- $(DP_ID), site_id: $(Site_ID), ST: $(ST) - $(FancySize) $(bytes.lng)--><!-- - $(Charset)--><!-- - $(score.lng): $(Score) [$(Pop_Rank)]--><!IF NAME="Content-Type" CONTENT="application/msword"><b class="mimetype">[DOC]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="application/zip"><b class="mimetype">[ZIP]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="application/rar"><b class="mimetype">[RAR]</b>&nbsp;
<!ELIKE NAME="Content-Type" CONTENT="audio/*"><b class="mimetype">[MP3]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="application/pdf"><b class="mimetype">[PDF]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="application/vnd.ms-excel"><b class="mimetype">[XLS]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="application/rtf"><b class="mimetype">[RTF]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="text/rtf"><b class="mimetype">[RTF]</b>&nbsp;
<!ELIKE NAME="Content-Type" CONTENT="image/*"><b class="mimetype">[IMG]</b>&nbsp;
<!ELIF NAME="Content-Type" CONTENT="application/x-shockwave-flash"><b class="mimetype">[SWF]</b>&nbsp;
<!ENDIF>
<a id="link$(Order)" onmousedown="return clk('$(DP_ID)','$(Order)');" class="restop" href="$(URL)"><!--
 --><!IF NAME="Title" CONTENT="[no title]">
	 <!IF NAME="MP3.Artist" CONTENT="">$(notitle.lng)<!ELSE>$(artist.lng): $&(MP3.Artist)<!ENDIF>
	 <!ELIKE NAME="Title" CONTENT="/tmp/ind*">$(notitle.lng)
    <!ELSE>$&(Title:cite:68)<!ENDIF><!--
--></a></div>
<div class="serp2" style="margin-left:20px;">
<!IF NAME="MP3.Album" CONTENT=""> <!ELSE><span class="result">$(album.lng): </span>
$&(MP3.Album)<br><!ENDIF>
<!IF NAME="MP3.Song" CONTENT=""> <!ELSE><span class="result">$(song.lng): </span>
$&(MP3.Song)<br><!ENDIF>
<!IF NAME="MP3.Year" CONTENT=""> <!ELSE><span class="result">$(year.lng): </span>
$&(MP3.Year)<br><!ENDIF>
<!IF NAME="Body" CONTENT=""><!IFNOT NAME="sea" CONTENT=""><div class="serp3"><small class="restop">$(summary.lng):</small> <small>$&(sea:cite:350)</small></div><!ENDIF><!ELSE><small>$&(Body:350)</small><br><!ENDIF></div>
<div class="serp2" style="margin-left:20px;"><small class="result">$&(URL.host:idnd)$&(URL.path:36)$&(URL.file:33:right)<IFNOT NAME="url.query_string" CONTENT="">$&(url.query_string:right:20)<!ENDIF>
	      <!IFNOT NAME="stored_href" CONTENT=""> &nbsp;&nbsp;<a onmousedown="return clk('$(DP_ID)','$(Order)');" href="$(stored_href)" title="$(lastmod.lng) $(Last-Modified)">$(stcopy.lng)</a><!ENDIF>
<!IFLIKE NAME="Content-Language" CONTENT="ru*"><!ELSE> &nbsp;&nbsp;<a href="http://translate.google.com/translate?js=y&amp;prev=_t&amp;hl=ru&amp;ie=UTF-8&amp;layout=1&amp;eotf=1&amp;u=$%(url)&amp;sl=auto&amp;tl=ru" title="$&(Content-Language) -> ru">$(translate.lng)</a><!ENDIF>
	      <!IFNOT NAME="sitelimit_href" CONTENT=""> &nbsp;&nbsp;<a href="$(self)$(sitelimit_href)" title="$(allresfrom.lng) $(url.host)">$(more.lng)</a>
	      <!ENDIF>
<!IFNOT NAME="geo.lat" CONTENT=""> &nbsp;&nbsp;<a href="/cgi-bin/gmap.cgi.ru?q=$%(q)&amp;c=$&(c)&amp;m=$&(m)&amp;sp=$&(sp)&amp;sy=$&(sy)&amp;s=$&(s)&amp;tmplt=gmap43.htm.ru&amp;s_tmplt=$&(tmplt)&amp;title=$%(title)&amp;geo.lat=$&(geo.lat)&amp;geo.lon=$&(geo.lon)">$(onmap.lng)</a><!ENDIF>
<!IFNOT NAME="Price" CONTENT=""><span class="result"> - $(price.lng): </span>$(Price)<!ENDIF>
<!IFNOT NAME="Release" CONTENT=""><span class="result"> - $(release.lng): </span>$(Release)<!ENDIF>
<!IFNOT NAME="ISBN" CONTENT=""><span class="result"> - ISBN: </span><a href="https://www.google.com/search?q=ISBN+$%(ISBN)">$(ISBN)</a><!ENDIF>
</small>
</div>
$(CL)</div>
<!-- n -->
<!--/res-->

<!--clone-->
<div><li><A HREF="$(URL)">$(URL:50)</A>,&nbsp; $(Last-Modified)</div>
<!--/clone-->

<!--navleft-->
<TD><A class="leftmenu" HREF="$(self)$(NH)">$(prev.lng)</A></TD>
<!--/navleft-->

<!--navbar0-->
<TD><b class="hl">$(NP)</b></TD>
<!--/navbar0-->

<!--navbar1-->
<TD><A class="leftmenu" HREF="$(self)$(NH)">$(NP)</A></TD>
<!--/navbar1-->

<!--navright-->
<TD><A class="leftmenu" HREF="$(self)$(NH)">$(next.lng)</A></TD>
<!--/navright-->

<!--navigator-->
<TABLE BORDER="0"><TR><TD>$(pages.lng):</TD>$(NL) $(NB) $(NR)</TR></TABLE>
<!--/navigator-->

<!--resbot-->
<!-- z -->
</td>
<td width="170" valign="top" align="left" style="border-left: 2px solid #f4f4f4">
<!IF NAME="label" CONTENT="sochi"><div id="similarArea">&nbsp;</div><div id="resultArea">&nbsp;</div>
<!ELSE><div id="resultArea">&nbsp;</div><div id="similarArea">&nbsp;</div><!ENDIF>
</td>
<td rowspan="2" valign="top" align="left" style="max-width: 14em; border-right:1px solid #f4f4f4;">
<!-- ads here -->
<!IF NAME="label" CONTENT="sochi"><!--br--><div id="rutubeArea">&nbsp;</div><!ENDIF>
</td>
</tr>
</table>
</div>
<div class="bottom">
<br>
<small title="$(WE)">&nbsp;<b>$(results.lng):</b>&nbsp;$(WS)</small>
<br><small>&nbsp;$(sortedby.lng):&nbsp;
<!IF NAME="s" CONTENT="DRP">
<a href="$(FirstPage)&amp;s=IRPD">$(isort.lng)</a>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=RPD">$(rsort.lng)</a>&nbsp;|&nbsp;<span class="hl">$(dsort.lng)</span>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=PRD">$(psort.lng)</a><!ELIF NAME="s" CONTENT="IRPD">
<span class="hl">$(isort.lng)</span>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=RPD">$(rsort.lng)</a>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=DRP">$(dsort.lng)</a>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=PRD">$(psort.lng)</a><!ELIF NAME="s" CONTENT="PRD">
<a href="$(FirstPage)&amp;s=IRPD">$(isort.lng)</a>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=RPD">$(rsort.lng)</a>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=DRP">$(dsort.lng)</a>&nbsp;|&nbsp;<span class="hl">$(psort.lng)</span><!ELSE>
<a href="$(FirstPage)&amp;s=IRPD">$(isort.lng)</a>&nbsp;|&nbsp;<span class="hl">$(rsort.lng)</span>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=DRP">$(dsort.lng)</a>&nbsp;|&nbsp;<a href="$(FirstPage)&amp;s=PRD">$(psort.lng)</a>
<!ENDIF><br></small>
<CENTER><TABLE BORDER="0" cellspacing="0" cellpadding="3"><TR><td>$(pages.lng):</td>$(NL) $(NB) $(NR)</TR></TABLE></CENTER>
</div>
<!--/resbot-->


<!--notfound-->
<!-- $(WA) -->
<div class="news">
<table width="100%" border="0" cellspacing="0" cellpadding="0">
<tr><td align="right">
<small>$(stime.lng):&nbsp;$(SearchTime) $(sec.lng)&nbsp;</small>
</td></tr>
</table>
</div>
<div class="news">
<small title="$(WE)">&nbsp;$(results.lng):&nbsp;$(WS)</small>
<!IFNOT NAME="Suggest_q" VALUE=""><br><small>&nbsp;<b>$(doyoumean.lng) &laquo;<a class="newcat" href="$(Suggest_url)">$(Suggest_q)</a>&raquo; ?</b></small>
<!ENDIF>
<br>
<br>
<b>$(notfound.lng)</b>
$(try.lng)
</div>
<br><br>
<table border="0" cellspacing="0" cellpadding="10"><tr>
<td><div id="similarArea" style="max-width: 14em;">&nbsp;</div></td>
<td><div id="resultArea" style="max-width: 14em;">&nbsp;</div></td>
<!IF NAME="label" CONTENT="sochi"><td><div id="rutubeArea" style="max-width: 14em; float:right;">&nbsp;</div></td><!ENDIF>
</tr></table>
<br>
<!--/notfound-->

<!--noquery-->
<center>
$(noquery.lng)
</center>
<!--/noquery-->

<!--error-->
<center>
<font color="#FF0000">$(error.lng)</font>
<p>
<b>$(E)</b>
</center>
<!--/error-->
