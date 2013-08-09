<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY dbstyle PUBLIC "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN" CDATA DSSSL>
]>

<style-sheet>
<style-specification use="docbook">
<style-specification-body>

<!-- ;; your stuff goes here... -->

(define %stylesheet% "datapark.css");


(define %html-prefix% 
  ;; Add the specified prefix to HTML output filenames
  "dpsearch-")

(define %use-id-as-filename%
  ;; Use ID attributes as name for component HTML files?
  #t)

(define %root-filename%
  ;; Name for the root HTML document
  "index")

(define %section-autolabel%
  ;; REFENTRY section-autolabel
  ;; PURP Are sections enumerated?
  ;; DESC
  ;; If true, unlabeled sections will be enumerated.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define %html-ext% 
  ;; REFENTRY html-ext
  ;; PURP Default extension for HTML output files
  ;; DESC
  ;; The default extension for HTML output files.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  ".en.html")

;;(define nochunks
  ;; REFENTRY nochunks
  ;; PURP Suppress chunking of output pages
  ;; DESC
  ;; If true, the entire source document is formatted as a single HTML
  ;; document and output on stdout.
  ;; (This option can conveniently be set with '-V nochunks' on the 
  ;; Jade command line).
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
;;  #f)

(define rootchunk
  ;; REFENTRY rootchunk
  ;; PURP Make a chunk for the root element when nochunks is used
  ;; DESC
  ;; If true, a chunk will be created for the root element, even though
  ;; nochunks is specified. This option has no effect if nochunks is not
  ;; true.
  ;; (This option can conveniently be set with '-V rootchunk' on the 
  ;; Jade command line).
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define make-entity?
  ;;
  #t)

(define html-index
  ;; REFENTRY html-index
  ;; PURP HTML indexing?
  ;; DESC
  ;; Turns on HTML indexing.  If true, then index data will be written
  ;; to the file defined by 'html-index-filename'.  This data can be
  ;; collated and turned into a DocBook index with bin/collateindex.pl.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define html-index-filename
  ;; Name of HTML index file
  "HTML.index")

(define html-manifest
  ;; REFENTRY html-manifest
  ;; PURP Write a manifest?
  ;; DESC
  ;; If not '#f' then the list of HTML files created by the stylesheet
  ;; will be written to the file named by 'html-manifest-filename'.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define html-manifest-filename
  ;; Name of HTML index file
  "HTML.manifest")

(define %refentry-xref-manvolnum%
  ;; REFENTRY refentry-xref-manvolnum
  ;; PURP Output manvolnum as part of RefEntry cross-reference?
  ;; DESC
  ;; If true, the manvolnum is used when cross-referencing RefEntrys, either
  ;; with XRef or CiteRefEntry.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define %olink-fragid%
  ;; Portion of the URL which identifies the fragment identifier
  "")

(define %olink-pubid% 
  ;; Portion of the URL which identifies the public identifier
  "")

(define %olink-resolution% 
  ;; URL script for OLink resolution
  "")

(define %olink-sysid% 
  ;; Portion of the URL which identifies the system identifier
  "")

(define %css-decoration%
  ;; Enable CSS decoration of elements
  #t)

(define %body-attr% 
  ;; What attributes should be hung off of BODY?
  (list
   (list "BGCOLOR" "#FFFFFF")
   (list "TEXT" "#000000")
   (list "LINK" "#0000C4")
   (list "VLINK" "#1200B2")
   (list "ALINK" "#C40000")))

(define %html-header-tags% 
  ;; What additional HEAD tags should be generated?
  '(
	("META" ("NAME" "Description") ("CONTENT" "DataparkSearch - Full Featured Web site Open Source Search Engine Software over the Internet and Intranet Web Sites Based on SQL Database. It is a Free search software covered by GNU license."))
        ("META" ("NAME" "Keywords") ("CONTENT" "shareware, freeware, download, internet, unix, utilities, search engine, text retrieval, knowledge retrieval, text search, information retrieval, database search, mining, intranet, webserver, index, spider, filesearch, meta, free, open source, full-text, udmsearch, website, find, opensource, search, searching, software, udmsearch, engine, indexing, system, web, ftp, http, cgi, php, SQL, MySQL, database, php3, FreeBSD, Linux, Unix, DataparkSearch, MacOS X, Mac OS X, Windows, 2000, NT, 95, 98, GNU, GPL, url, grabbing"))
   )
)

;;(define %html40%
  ;; Generate HTML 4.0
;;  #t)

(define %footnotes-at-end%
  ;; Should footnotes appear at the end of HTML pages?
  #t)

(define %generate-part-toc% 
  ;; Should a Table of Contents be produced for Parts?
  #t)

(define %generate-article-toc% 
  ;; Should a Table of Contents be produced for Articles?
  #t)

(define ($html-body-start$)
	(make empty-element gi: "!--#include virtual=\"body-before.html\"--"))
(define ($html-body-end$)
	(make empty-element gi: "!--#include virtual=\"body-after.html\"--"))


</style-specification-body>
</style-specification>
<external-specification id="docbook" document="dbstyle">
</style-sheet>
