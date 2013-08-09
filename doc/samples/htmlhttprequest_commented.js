/*

HTMLHttpRequest v1.0 beta3
(c) 2001-2006 Angus Turnbull, TwinHelix Designs http://www.twinhelix.com

Licensed under the CC-GNU LGPL, version 2.1 or later:
http://creativecommons.org/licenses/LGPL/2.1/
This is distributed WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/



// Some common event API code I use. Syntax:
//   addEvent(object_reference, 'name_of_event', function_reference, legacy);
// 'name_of_event' is without the 'on' prefix, e.g. 'load', 'click' or 'mouseover'.
// 'legacy' disables addEventListener if true.
// You can add multiple events to one object, and in MSIE all are removed onunload.

if (typeof addEvent != 'function')
{
 var addEvent = function(o, t, f, l)
 {
  var d = 'addEventListener', n = 'on' + t, rO = o, rT = t, rF = f, rL = l;
  if (o[d] && !l) return o[d](t, f, false);
  if (!o._evts) o._evts = {};
  if (!o._evts[t])
  {
   o._evts[t] = o[n] ? { b: o[n] } : {};
   o[n] = new Function('e',
    'var r = true, o = this, a = o._evts["' + t + '"], i; for (i in a) {' +
     'o._f = a[i]; r = o._f(e||window.event) != false && r; o._f = null;' +
     '} return r');
   if (t != 'unload') addEvent(window, 'unload', function() {
    removeEvent(rO, rT, rF, rL);
   });
  }
  if (!f._i) f._i = addEvent._i++;
  o._evts[t][f._i] = f;
 };
 addEvent._i = 1;
 var removeEvent = function(o, t, f, l)
 {
  var d = 'removeEventListener';
  if (o[d] && !l) return o[d](t, f, false);
  if (o._evts && o._evts[t] && f._i) delete o._evts[t][f._i];
 };
}

function cancelEvent(e, c)
{
 e.returnValue = false;
 if (e.preventDefault) e.preventDefault();
 if (c)
 {
  e.cancelBubble = true;
  if (e.stopPropagation) e.stopPropagation();
 }
};



// SYNTAX INSTRUCTIONS for HTMLHttpRequest:
//
// function callback_function(DOMDocument) { /* Parse DOM document here */ }
//
// var objectName = new HTMLHttpRequest('objectName', callback_function);
//
// Create an instance of an HTMLHttpRequest object, and pass its own name as a string,
// a reference to a callback function, and a pathname to a blank HTML document.
// The callback function will be called on load/submit completion with parameters:
//  1) A reference to the loaded DOM document (which you may then parse).
//  2) The text content of the loaded document.
//  2) The requested URI.
// NOTE: All requested documents must reside on the same domain as this document!
//
// Available methods are:
//
// objectName.load('file.html');
//
// This will issue a GET request to the server to return a named file.
//
// objectName.submit(form_reference, event_object);
//
// This will capture a form's submission and redirect it to a background POST or GET
// request, respecting the form's 'method' attribute and its 'action' URI.
// Pass it a reference to the form's DOM node, and the event object from the submittal.
// Note that the form must be *already* in the process of submission when this is called.
// It is therefore suggested that you call it from within an ONSUBMIT handler on a form.

function HTMLHttpRequest(myName, callback) { with (this)
{
 this.myName = myName;
 this.callback = callback;

 // 'xmlhttp': Our preferred request object. Accepted wherever HTML is sold.
 this.xmlhttp = null;
 // 'iframe' is our fallback: a reference to a dynamically created IFRAME buffer.
 this.iframe = null;
 window._ifr_buf_count |= 0;
 this.iframeID = 'iframebuffer' + window._ifr_buf_count++;
 // A loading flag, set to the requested URI when loading.
 this.loadingURI = '';

 // Attempt to init an XMLHttpRequest object where supported.
 // Note: MSIE7 is best with the IFRAME method, so exclude IE here.
 if (window.XMLHttpRequest && !window.ActiveXObject) xmlhttp = new XMLHttpRequest();

 if (!xmlhttp)
 {
  // In MSIE or browsers with no XMLHttpRequest support: fallback to IFRAMEs buffers.
  // I'm using IFRAMEs in MSIE due to XMLHTTP not parsing text/html to a DOM.
  // Also used in IE5/Mac, Opera 7.x, Safari <1.2, some Konqueror versions, etc.
  if (document.createElement && document.documentElement &&
   (window.opera || navigator.userAgent.indexOf('MSIE 5.0') == -1))
  {
   var ifr = document.createElement('iframe');
   ifr.setAttribute('id', iframeID);
   ifr.setAttribute('name', iframeID);
   // Hide with visibility instead of display to fix Safari bug.
   ifr.style.visibility = 'hidden';
   ifr.style.position = 'absolute';
   ifr.style.width = ifr.style.height = ifr.borderWidth = '0px';
   iframe = document.getElementsByTagName('body')[0].appendChild(ifr);
  }
  else if (document.body && document.body.insertAdjacentHTML)
  {
   // IE5.0 doesn't like createElement'd frames (won't script them) and IE4 just plain
   // doesn't support it. Luckily, this will fix them both:
   document.body.insertAdjacentHTML('beforeEnd', '<iframe name="' + iframeID +
    '" id="' + iframeID + '" style="display: none"></iframe>');
  }
  // This helps most IE versions regardless of the creation method:
  if (window.frames && window.frames[iframeID]) iframe = window.frames[iframeID];
  iframe.name = iframeID;
 }

 // TODO: Put in further DOM3 LSParser support as a transport layer...?

 return this;
}};



HTMLHttpRequest.prototype.parseForm = function(form) { with (this)
{
 // Parses a form DOM reference to an escaped string suitable for GET/POSTing.

 var str = '', gE = 'getElementsByTagName', inputs = [
  (form[gE] ? form[gE]('input') : form.all ? form.all.tags('input') : []),
  (form[gE] ? form[gE]('select') : form.all ? form.all.tags('select') : []),
  (form[gE] ? form[gE]('textarea') : form.all ? form.all.tags('textarea') : [])
 ];

 // Loop through each list of tags, constructing our string.
 for (var i = 0; i < inputs.length; i++)
  for (j = 0; j < inputs[i].length; j++)
   if (inputs[i][j])
   {
    var plus = '++'.substring(0,1); // CodeTrim fix.
    str += escape(inputs[i][j].getAttribute('name')).replace(plus, '%2B') +
     '=' + escape(inputs[i][j].value).replace(plus, '%2B') + '&';
   }

 // Strip trailing ampersand, because we can :)
 return str.substring(0, str.length - 1);
}};



HTMLHttpRequest.prototype.xmlhttpSend = function(uri, formStr) { with (this)
{
 // Use XMLHttpRequest to asynchronously open a URI, and optionally POST a provided
 // form string if any (otherwise, performs a GET).

 xmlhttp.open(formStr ? 'POST' : 'GET', uri, true);
 xmlhttp.onreadystatechange = function() {
  if (xmlhttp.readyState == 4)
  {
   // If you are getting an error where either of these value are null, try changing
   // the MIME type returned by the server: setting it to text/xml usually works well!
   if (callback) callback(xmlhttp.responseXML, xmlhttp.responseText, loadingURI);
   loadingURI = '';
  }
 };
 if (formStr && xmlhttp.setRequestHeader)
  xmlhttp.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
 // You might need to customise this.
 if (xmlhttp.overrideMimeType)
  xmlhttp.overrideMimeType((/\.txt/i).test(uri) ? 'text/plain' : 'text/xml');
 xmlhttp.send(formStr);
 loadingURI = uri;
 return true;
}};


HTMLHttpRequest.prototype.iframeSend = function(uri, formRef) { with (this)
{
 // Routes a request through our hidden IFRAME. Pass a URI, and optionally a
 // reference to a submitting form. Requires proprietary 'readyState' property.

 if (!document.readyState) return false;

 // Opera fix: force the frame to render before setting it as a target.
 if (document.getElementById) var o = document.getElementById(iframeID).offsetWidth;

 // Either route the form submission to the IFRAME (easy!)...
 if (formRef) formRef.setAttribute('target', iframeID);
 else
 {
  // ...or load the provided URI in the IFRAME, checking for browser bugs:
  // 1) Safari only works well using 'document.location'.
  // 2) Opera needs the 'src' explicitly set!
  // 3) Konqueror 3.1 seems to think ifrDoc's location = window.location, so watch that too.
  var ifrDoc = iframe.contentDocument || iframe.document;

  if (!window.opera && ifrDoc.location &&
   ifrDoc.location.href != location.href) ifrDoc.location.replace(uri);
  else iframe.src = uri;
 }

 // Either way, set the loading flag and start the readyState checking loop.
 // Opera likes a longer initial timeout with multiple frames running...
 loadingURI = uri;
 setTimeout(myName + '.iframeCheck()', (window.opera ? 250 : 100));
 return true;
}};


HTMLHttpRequest.prototype.iframeCheck = function() { with (this)
{
 // Called after a timeout, periodically calls itself until the load is complete.
 // Get a reference to the loaded document, using either W3 contentDocument or IE's DOM.

 doc = iframe.contentDocument || iframe.document;
 // Check the IFRAME's .readyState property and callback() if load complete.
 // IE4 only seems to advance to 'interactive' so let it proceed from there.
 var il = iframe.location, dr = doc.readyState;
 if ((il && il.href ? il.href.match(loadingURI.replace("\?", "\\?")) : 1) &&
     (dr == 'complete' || (!document.getElementById && dr == 'interactive')))
 {
  var cbDoc = doc.documentElement || doc;
  if (callback) callback(cbDoc,
   (cbDoc.innerHTML || (cbDoc.body ? cbDoc.body.innerHTML : '')), loadingURI);
  loadingURI = '';
 }
 else setTimeout(myName + '.iframeCheck()', 50);
}};


// *** PUBLIC METHODS ***

HTMLHttpRequest.prototype.load = function(uri) { with (this)
{
 // Call with a URI to load a plain text document.

 if (!uri || (!xmlhttp && !iframe)) return false;
 // Route the GET through an available transport layer.
 if (xmlhttp) return xmlhttpSend(uri, '');
 else if (iframe) return iframeSend(uri, null);
 else return false;
}};


HTMLHttpRequest.prototype.submit = function(formRef, evt) { with (this)
{
 // Pass a reference to a (submitting) form DOM node and an event object.

 evt = evt || window.event;
 if (!formRef || (!xmlhttp && !iframe)) return false;

 // Retrieve form information then decide what to do with it.
 var method = formRef.getAttribute('method'), uri = formRef.getAttribute('action');

 if (method && method.toLowerCase() == 'post')
 {
  // Send the URI and either a parsed form or a form reference to the transports.
  // Note we only cancel form for XMLHTTP, as IFRAMEs still need it to submit.
  if (xmlhttp) { cancelEvent(evt); return xmlhttpSend(uri, parseForm(formRef)) }
  else if (iframe) return iframeSend(uri, formRef);
  else return false;
 }
 else
 {
  // For GET requests, append ?querystring or &querystring to the GET uri and
  // forward it to the load() function. Always cancel form submit().
  cancelEvent(evt);
  return load(uri + (uri.indexOf('?') == -1 ? '?' : '&') + parseForm(formRef));
 }
}};












// SYNTAX INSTRUCTIONS for RemoteFileLoader:
//
// var objectName = new RemoteFileLoader('objectName');
//
// This is our "Threading" layer that automatically creates, allocates and processes
// HTMLHttpRequest objects, so you can easily issue concurrent requests to a server
// and load areas of content into your page without writing complex management code.
// Pass its own name in quotes and a pathname to a blank HTML file as above.
//
// Available methods and properties are:
//
// objectName.loadInto('file.html', 'id-of-target');
//
// This loads a specified file into an element with an ID="id-of-target" attribute.
//
// objectName.submitInto(form_reference, 'id-of-target', event_object);
//
// This will capture a form's submission and load within the specified target element.
// Pass it a reference to the form's DOM node, and the event object from the submittal.
// Note that the form must be *already* in the process of submission when this is called.
// It is therefore suggested that you call it from within an ONSUBMIT handler on a form.
//
// objectName.onload = function_reference;
//
// Specifies a function reference to be called whenever a file has loaded.
// It is passed a reference to the loaded document, the loaded URI, and destination ID.
//
// Note that for both loadInto() and submitInto(), subsequent requests targeted at the
// same element will cancel earlier requests if they are still loading.
// Also, all loaded files must reside on the same domain as the requesting document.

function RemoteFileLoader(myName)
{
 this.myName = myName;
 this.threads = [];
 this.loadingIDs = {};
 this.onload = null;
};


RemoteFileLoader.prototype.getThread = function(destId) { with (this)
{
 // Locates a thread that's EITHER: loading into the specified destination, OR: idle.
 // If none are match that condition, create a new one.

 var thr = -1;

 for (var id in loadingIDs)
 {
  // Same destination?
  if (id == destId)
  {
   thr = loadingIDs[id];
   break;
  }
 }
 if (thr == -1) for (var t = 0; t < threads.length; t++)
 {
  // Idle?
  if (!threads[t].loadingURI)
  {
   thr = t;
   break;
  }
 }
 if (thr == -1)
 {
  // Create a new HTMLHttpRequest with its own name as a reference to its array entry,
  // and no callback function as yet ;).
  thr = threads.length;
  threads[thr] = new HTMLHttpRequest(myName + '.threads[' + thr + ']', null);
  // Record this thread as loading for this destination, so it can be aborted later.
  loadingIDs[destId] = thr;
 }

 // Assign our callback function; it passed the destination to copyContent.
 threads[thr].callback = new Function('doc', 'text', 'uri', 'with (' + myName + ') { ' +
  'copyContent(doc, text, "' + destId + '"); if (onload) onload(doc, uri, "' + destId + '") }');

 return threads[thr];
}};


RemoteFileLoader.prototype.loadInto = function(uri, destId)
{
 // Pass the file onto the load method of the selected thread.
 return this.getThread(destId).load(uri);
};


RemoteFileLoader.prototype.submitInto = function(formRef, destId, event)
{
 // Pass the file onto the submit method of the selected thread.
 return this.getThread(destId).submit(formRef, event);
};


RemoteFileLoader.prototype.copyContent = function(docDOM, docText, destId)
{
 // This copies the <body> content of the loaded DOM document into an element in the
 // current page with a specified ID.

 // Retrieve references to the loaded BODY. You might want to modify this so that you
 // load content from <div id="content"> within the loaded document perhaps?
 var src = docDOM ?
  (docDOM.getElementsByTagName ?
   docDOM.getElementsByTagName('body')[0] :
    (docDOM.body ? docDOM.body : null) ) :
     null;
 var dest = document.getElementById ? document.getElementById(destId) :
  (document.all ? document.all[destId] : null);
 if (!dest || (!src && !docText)) return;
 // innerHTML is still a little more reliable than importNode across browsers.
 if (src && src.innerHTML) dest.innerHTML = src.innerHTML;
 else if (src && document.importNode)
 {
  while (dest.firstChild) dest.removeChild(dest.firstChild);
  for (var i = 0; i < src.childNodes.length; i++)
   dest.appendChild(document.importNode(src.childNodes.item(i), true));
 }
	else if (docText)
	{
	 if (docText.match(/(<body>)(.*)(<\/body>)/i)) docText = RegExp.$2;
		// You might want to do some post-processing here if you are rendering
		// plain text in an XHTML document, for instance to keep line breaks:
		//docText = '<pre>' + docText + '</pre>';
		dest.innerHTML = docText;
	}
};