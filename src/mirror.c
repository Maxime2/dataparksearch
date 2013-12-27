/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA 
*/

#include "dps_common.h"
#include "dps_proto.h"
#include "dps_utils.h"
#include "dps_vars.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif


__C_LINK int __DPSCALL DpsMirrorGET(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_URL *url) {
     int       size = 0;
     int       fbody, fheader;
     char      *str, *estr;
     struct stat    sb;
     time_t         nowtime;
     size_t          str_len, estr_len;
     size_t         allocated_size = 32;
     int       have_headers=0;
     int       mirror_period=DpsVarListFindInt(&Doc->Sections,"MirrorPeriod",-1);
     const char     *mirror_data = DpsVarListFindStr(&Doc->Sections,"MirrorRoot",NULL);
     const char     *mirror_hdrs = DpsVarListFindStr(&Doc->Sections,"MirrorHeadersRoot",NULL);
     const char     *accept_lang = DpsVarListFindStr(&Doc->Sections, "Content-Language", NULL);
     
     if (mirror_data == NULL && mirror_hdrs == NULL) return DPS_MIRROR_NOT_FOUND;

     if (accept_lang == NULL) {
       accept_lang = DpsVarListFindStr(&Doc->RequestHeaders, "Accept-Language", NULL);
     }

     Doc->Buf.size = 0;
     nowtime = Indexer->now;
     
     if(mirror_period <= 0)return(DPS_MIRROR_NOT_FOUND);
     
     /* MirrorRoot is not specified, nothing to do */
     if(mirror_data == NULL) return(DPS_MIRROR_NOT_FOUND);

     estr_len = 64 + 3 * dps_strlen(DPS_NULL2EMPTY(url->filename)) + 3 * dps_strlen(DPS_NULL2EMPTY(accept_lang));
     estr_len += 3 * dps_strlen(DPS_NULL2EMPTY(url->query_string));
     
     str_len = 128 + dps_strlen(mirror_data) + ((mirror_hdrs) ? dps_strlen(mirror_hdrs) : 0) + dps_strlen(DPS_NULL2EMPTY(url->schema)) +
       dps_strlen(DPS_NULL2EMPTY(url->hostname)) + dps_strlen(DPS_NULL2EMPTY(url->path)) + estr_len;

     if ((str = (char*)DpsMalloc(str_len + 1)) == NULL) return DPS_MIRROR_NOT_FOUND;
     if ((estr = (char*)DpsMalloc(estr_len + 1)) == NULL) {
       DPS_FREE(str);
       return DPS_MIRROR_NOT_FOUND;
     }

     dps_snprintf(str, str_len, "%s%s%s%s", dps_strlen(DPS_NULL2EMPTY(url->filename)) ? url->filename : "index.html",
		  DPS_NULL2EMPTY(url->query_string), (accept_lang == NULL) ? "" : ".", (accept_lang == NULL) ? "" : accept_lang);
     DpsEscapeURL(estr, str);

     dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s%s%s.body", mirror_data,
               DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DPS_NULL2EMPTY(url->path), estr);

     body_again:

     if ((fbody = DpsOpen2(str, O_RDONLY | DPS_BINARY)) == -1){
	 if (errno == ENAMETOOLONG) {
	     dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s"DPSSLASHSTR"url_id_%d.body", mirror_data, 
			  DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DpsURL_ID(Doc, NULL));
	     goto body_again;
	 }
	 DpsLog(Indexer, DPS_LOG_EXTRA, "Mirror file %s not found", str);
	 DPS_FREE(estr); DPS_FREE(str);
	 return DPS_MIRROR_NOT_FOUND;
     }

     /* Check on file mtime > days ? return */
     if (fstat(fbody, &sb)) {
             DPS_FREE(estr); DPS_FREE(str);
          return DPS_MIRROR_NOT_FOUND;
     }
     if (nowtime > sb.st_mtime + mirror_period ) {
          DpsClose(fbody);
          DpsLog(Indexer, DPS_LOG_EXTRA, "%s is older then %d secs", str, mirror_period);
          DPS_FREE(estr); DPS_FREE(str);
          return DPS_MIRROR_EXPIRED;
     }
     allocated_size += sb.st_size;

     if(mirror_hdrs){
          dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s%s%s.header", 
                mirror_hdrs, DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DPS_NULL2EMPTY(url->path), estr);

     hdrs_again:

          if ((fheader = DpsOpen2(str, O_RDONLY | DPS_BINARY)) >= 0) {
	      if (errno == ENAMETOOLONG) {
		  dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s"DPSSLASHSTR"url_id_%d.header", mirror_data, 
			       DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DpsURL_ID(Doc, NULL));
		  goto hdrs_again;
	      }
	      if (fstat(fheader, &sb)) {
		  DPS_FREE(estr); DPS_FREE(str);
		  return DPS_MIRROR_NOT_FOUND;
	      }
	      allocated_size += sb.st_size;
	      if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, allocated_size + 1)) == NULL) {
		  DPS_FREE(estr); DPS_FREE(str); 
		  Doc->Buf.allocated_size = 0;
		  return DPS_MIRROR_NOT_FOUND;
	      } 
	      Doc->Buf.allocated_size = allocated_size;
	      size = read(fheader, Doc->Buf.buf, Doc->Buf.allocated_size);
	      DpsClose(fheader);
	      dps_strcpy(Doc->Buf.buf + size, "\r\n\r\n");
	      have_headers = 1;
          }
     } else {
       if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, allocated_size + 1)) == NULL) {
	 DPS_FREE(estr); DPS_FREE(str);
	 Doc->Buf.allocated_size = 0;
	 return DPS_MIRROR_NOT_FOUND;
       }
       Doc->Buf.allocated_size = allocated_size;
     }
     if(!have_headers){
          /* header file not found   */
          /* get body as file method */

          sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
          sprintf(DPS_STREND(Doc->Buf.buf),"\r\n");
     }

     DPS_FREE(estr); DPS_FREE(str);
     Doc->Buf.content = DPS_STREND(Doc->Buf.buf);
     size = read(fbody, Doc->Buf.content, Doc->Buf.allocated_size - (Doc->Buf.content - Doc->Buf.buf));
     DpsClose(fbody);
     if (size < 0) return size;
     /* Append trailing 0 */
     Doc->Buf.content[size] = '\0';
     /* Calculate size */
     Doc->Buf.size = (Doc->Buf.content - Doc->Buf.buf) + size;
     return DPS_OK;
}


__C_LINK int __DPSCALL DpsMirrorPUT(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_URL *url, const char *suffix) {
     int       fd;
     char      *str, *estr;
     size_t    str_len, estr_len, buf_len;
     ssize_t   size;
     const char     *mirror_data = DpsVarListFindStr(&Doc->Sections, "MirrorRoot", NULL);
     const char     *mirror_hdrs = DpsVarListFindStr(&Doc->Sections, "MirrorHeadersRoot", NULL);
     const char     *accept_lang = DpsVarListFindStr(&Doc->Sections, "Content-Language", NULL);
     char            *token, savechar = CR_CHAR;

     if (mirror_data == NULL && mirror_hdrs == NULL) return DPS_OK;

     if (accept_lang == NULL) {
       accept_lang = DpsVarListFindStr(&Doc->RequestHeaders, "Accept-Language", NULL);
     }
     
     /* Cut HTTP response header first        */
     for(token = Doc->Buf.buf; *token; token++){
          if(!strncmp(token, "\r\n\r\n", 4)){
               *token = '\0';
               Doc->Buf.content = token + 4;
               savechar = CR_CHAR;
               break;
          }else
          if(!strncmp(token, "\n\n", 2)){
               *token = '\0';
               Doc->Buf.content = token + 2;
               savechar = NL_CHAR;
               break;
          }
     }

     estr_len = 64 + 3 * dps_strlen(DPS_NULL2EMPTY(url->filename)) + 3 * dps_strlen(DPS_NULL2EMPTY(accept_lang));
     estr_len += 3 * dps_strlen(DPS_NULL2EMPTY(url->query_string));
     if (suffix != NULL) estr_len += dps_strlen(suffix);
     
     str_len = 128 + dps_strlen(DPS_NULL2EMPTY(mirror_data)) + dps_strlen(DPS_NULL2EMPTY(mirror_hdrs)) + dps_strlen(DPS_NULL2EMPTY(url->schema)) +
       dps_strlen(DPS_NULL2EMPTY(url->hostname)) + dps_strlen(DPS_NULL2EMPTY(url->path)) + estr_len;

     if ((str = (char*)DpsMalloc(str_len + 1)) == NULL) {
       *token = savechar;
       return DPS_MIRROR_CANT_BUILD;
     }
     if ((estr = (char*)DpsMalloc(estr_len + 1)) == NULL) {
       DPS_FREE(str);
       *token = savechar;
       return DPS_MIRROR_CANT_BUILD;
     }

     dps_snprintf(str, str_len, "%s%s%s%s", (dps_strlen(DPS_NULL2EMPTY(url->filename))) ? url->filename : "index.html",
		  DPS_NULL2EMPTY(url->query_string), (accept_lang == NULL) ? "" : ".", (accept_lang == NULL) ? "" : accept_lang);
     DpsEscapeURL(estr, str);

     /* Put Content if MirrorRoot is specified */
     if (mirror_data) {
       dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s%s", mirror_data, 
                    DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DPS_NULL2EMPTY(url->path));

       if(DpsBuild(str, 0755) != 0){
	    DpsLog(Indexer, DPS_LOG_ERROR, "Can't create dir %s", str);
	    *token = savechar;
	    DPS_FREE(estr); DPS_FREE(str);
	    return DPS_MIRROR_CANT_BUILD;
       }

       if (url->path == NULL || url->path[0] == '\0') dps_strcat(str, DPSSLASHSTR);
       dps_strcat(str, estr);
     body_again:

       if (suffix != NULL) {
	 dps_strcat(str, ".");
	 dps_strcat(str, suffix);
       }
       dps_strcat(str, ".body");

       if ((fd = DpsOpen3(str, O_CREAT | O_WRONLY | DPS_BINARY, DPS_IWRITE)) == -1) {
	   if (errno == ENAMETOOLONG) {
	       dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s"DPSSLASHSTR"url_id_%d", mirror_data, 
			    DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DpsURL_ID(Doc, NULL));
	       goto body_again;
	   }
               dps_strerror(Indexer, DPS_LOG_EXTRA, "Can't open mirror file %s\n", str);
               *token = savechar;
               DPS_FREE(estr); DPS_FREE(str);
               return DPS_MIRROR_CANT_OPEN;
       }
       size = write(fd, Doc->Buf.content, Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf));
       DpsClose(fd);
     }

     /* Put Headers if MirrorHeadersRoot is specified */
     if (mirror_hdrs && (suffix == NULL)) {
       dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s%s", mirror_hdrs, DPS_NULL2EMPTY(url->schema),
                    DPS_NULL2EMPTY(url->hostname), DPS_NULL2EMPTY(url->path));

       if(DpsBuild(str, 0755) != 0){
	 DpsLog(Indexer, DPS_LOG_ERROR, "Can't create dir %s", str);
	 *token = savechar;
	 DPS_FREE(estr); DPS_FREE(str);
	 return DPS_MIRROR_CANT_BUILD;
       }

       if (url->path == NULL || url->path[0] == '\0') dps_strcat(str, DPSSLASHSTR);
       dps_strcat(str, estr);
     hdrs_again:

       dps_strcat(str, ".header");
       
       if ((fd = DpsOpen3(str, O_CREAT | O_WRONLY | DPS_BINARY, DPS_IWRITE)) == -1) {
	   if (errno == ENAMETOOLONG) {
	       dps_snprintf(str, str_len, "%s"DPSSLASHSTR"%s"DPSSLASHSTR"%s"DPSSLASHSTR"url_id_%d", mirror_data, 
			    DPS_NULL2EMPTY(url->schema), DPS_NULL2EMPTY(url->hostname), DpsURL_ID(Doc, NULL));
	       goto hdrs_again;
	   }
	   dps_strerror(Indexer, DPS_LOG_EXTRA, "Can't open mirror file %s\n", str);
	   *token = savechar;
	   DPS_FREE(estr); DPS_FREE(str);
	   return DPS_MIRROR_CANT_OPEN;
       }
       size = write(fd, Doc->Buf.buf, buf_len = dps_strlen(Doc->Buf.buf));
       if (size < 0) {
	   dps_strerror(Indexer, DPS_LOG_ERROR, "Error writing mirror file %", str);
       }
       DpsClose(fd);
     }
     DPS_FREE(estr); DPS_FREE(str);
     *token = savechar;
     return DPS_OK;
}
