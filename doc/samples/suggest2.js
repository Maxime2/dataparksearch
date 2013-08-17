//////////////////////////////////////////
var disable = 0;
var curpos = 0;
var nsugg = 0;
var g_slabel = '';
var g_str = '';
var qsimilar = '';
Suggests = new Array();
var old_str = '';
var old_slabel = 'qsimilar';
var SuggestLoader = new HTMLHttpRequest('SuggestLoader', SuggestCopyContent);
//////////////////////////////////////////
function utf8_encode ( string ) {
  string = (string+'').replace(/\r\n/g, "\n").replace(/\r/g, "\n");
 
  var utftext = "";
  var start, end;
  var stringl = 0;
 
  start = end = 0;
  stringl = string.length;
  for (var n = 0; n < stringl; n++) {
    var c1 = string.charCodeAt(n);
    var enc = null;
 
    if (c1 < 128) {
      end++;
    } else if((c1 > 127) && (c1 < 2048)) {
      enc = String.fromCharCode((c1 >> 6) | 192) + String.fromCharCode((c1 & 63) | 128);
    } else {
      enc = String.fromCharCode((c1 >> 12) | 224) + String.fromCharCode(((c1 >> 6) & 63) | 128) + String.fromCharCode((c1 & 63) | 128);
    }
    if (enc != null) {
      if (end > start) {
	utftext += string.substring(start, end);
      }
      utftext += enc;
      start = end = n+1;
    }
  }
  if (end > start) {
    utftext += string.substring(start, string.length);
  }
  return utftext;
}


function SuggestCopyContent(domXML, domDoc, uri) {
  var destId = 'search_suggest';
  var dest = document.getElementById ? document.getElementById(destId) : (document.all ? document.all[destId] : null);
  var clie = document.getElementById ? document.getElementById('q') : (document.all ? document.all['q'] : null);
  if (disable) return;
  if (!dest) return;
  if (dest.innerHTML && domDoc) {
    	nsugg = 0;
    	curpos = 0;
	var te = document.createElement('div');
	te.innerHTML = domDoc;
	var need = 0;
        for (var childItem in te.childNodes) {
      		if (te.childNodes[childItem].nodeName == "DIV") {
			var str = ''+te.childNodes[childItem].innerHTML.replace(/<[^>]*>/g, "");
			if (str.toString() != '') {
				need++;
				break;
			}
		}
	}
	if (need) {
		dest.innerHTML = domDoc;
		if (clie) dest.style.width = clie.clientWidth + 'px';
		dest.style.display = 'block';

		for (var childItem in dest.childNodes) {
      			if (dest.childNodes[childItem].nodeName == "DIV") {
				var str = ''+dest.childNodes[childItem].innerHTML.replace(/<[^>]*>/g, "");
				if (str.toString() != '') {
					nsugg++;
					Suggests[nsugg] = dest.childNodes[childItem];
				}
      			}
		}
	}
    	if (nsugg == 0 && g_slabel != old_slabel) { 
  		SuggestLoader.load('/cgi-bin/search.cgi?q=' + g_str + '&m=any&sp=1&sy=0&GroupBySite=yes&s=IRPD&&ps=10&tmplt=suggest.htm&label='+g_slabel);
		old_slabel = g_slabel;
    	}
  } else {
    dest.style.display = 'none';
  }
}

function searchSuggest(slabel) { 
  if (disable) return;
  with(SuggestLoader) {
    if (xmlhttp.readyState == 4 || xmlhttp.readyState == 0) {	
      var clie = document.getElementById ? document.getElementById('q') : (document.all ? document.all['q'] : null);
      if (!clie) return;
      if (disable) return;
      var str = clie.value;
      if (str.length > 0) {
	str = escape(utf8_encode(str.toString()));
	if (str != old_str) {
	  old_str = str;
    	  curpos = 0;
	  g_str = str;
		if (slabel == null) {
			g_slabel = '';
		        qsimilar='qsimilar';
		} else {
			g_slabel = slabel;
		    if (slabel == 'momentum') {
			qsimilar='msimilar';
		    } else {
			qsimilar='qsimilar';
		    }
		}
		old_slabel = 'qsimilar';
	  SuggestLoader.load('/cgi-bin/search.cgi?q=' + str + '&m=all&sp=1&sy=1&GroupBySite=no&s=IRPD&&ps=10&tmplt=suggest.htm&label='+qsimilar);
	}
      }
    }
  }
}
//Mouse over function
function suggestOver(div_value) {
	div_value.className = 'suggest_link_over';
	if (curpos > 0) {
	  Suggests[curpos].className = 'suggest_link';
	}
	for (var i = 1; i <= nsugg; i++) {
		if (Suggests[i] == div_value) {
			curpos = i;
			break;
		}
	}
}
//Mouse out function
function suggestOut(div_value) {
	div_value.className = 'suggest_link';
}
//Click function
function setSearch(value) {
 	var destId = 'search_suggest';
 	var dest = document.getElementById ? document.getElementById(destId) : (document.all ? document.all[destId] : null);
	var clie = document.getElementById ? document.getElementById('q') : (document.all ? document.all['q'] : null);
	clie.value = value.replace(/<[^>]*>/g, "");
	dest.style.display='none';
	disable = 1;
//	clie.focus();
	var p = clie.parentNode;
	while (p.nodeName != "FORM") p = p.parentNode;
	p.submit();
}
//Hide function
function HideSuggest() {
 	var destId = 'search_suggest';
 	var dest = document.getElementById ? document.getElementById(destId) : (document.all ? document.all[destId] : null);
	var clie = document.getElementById ? document.getElementById('q') : (document.all ? document.all['q'] : null);
	dest.style.display='none';
	clie.focus();
	disable = 1 - disable;
	curpos = 0;
	return true;
}
// Keyboard related
document.onkeydown = register;
function register(e) {
        if (!e) e = window.event;
        var k = e.keyCode;
	if (k == 27) { // Esc
		HideSuggest();
	}
	if (k == 38) { // Up arrow
		if (curpos > 0) Suggests[curpos].className = 'suggest_link';
		curpos--;
		if (curpos < 0) curpos = nsugg - 1;
		if (curpos > 0) {
			Suggests[curpos].className = 'suggest_link_over';
			Suggests[curpos].focus();
		}
	}
	if (k == 40) { // Down arrow
		if (nsugg > 0) {
			if (curpos > 0) Suggests[curpos].className = 'suggest_link';
			curpos++;
			if (curpos > nsugg - 1) curpos = 0;
			Suggests[curpos].className = 'suggest_link_over';
			Suggests[curpos].focus();
		}
	}
	if (k == 13) { // Enter
		disable = 1;
		if (curpos > 0) setSearch(Suggests[curpos].innerHTML);
	}
//        if (e.ctrlKey) {
//                if (k == 37 || k == 39) {
//                }
//                if (k == 38 || k == 40) {
//                }
//        }
}

