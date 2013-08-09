/* Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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

#include "dps_config.h"
#include <stdio.h>
#include <string.h>
#include "dps_uniconv.h"
#include "dps_charsetutils.h"
#include "dps_unidata.h"


void __DPSCALL DpsConvInit(DPS_CONV *cnv, DPS_CHARSET *from, DPS_CHARSET *to, const char *CharsToEscape, int fl) {
  cnv->from=from;
  cnv->to=to;
  cnv->flags=fl;
  cnv->ibytes=0;
  cnv->obytes=0;
  cnv->icodes = cnv->ocodes = 1; /* default for old functions */
  cnv->istate = cnv->ostate = 0; /* default state */
  cnv->CharsToEscape = CharsToEscape;
}

static int dps_ENTITYprint(char *dest, char sym, dpsunicode_t n) {
  dpsunicode_t mask = 10000000;
  register dpsunicode_t dig;
  int was_lead = 0;
  char *d = dest;
  *d = sym; d++;
  *d = '#'; d++;
  while(mask > 0) {
    dig = n / mask;
    if (dig > 0 || was_lead) {
      *d = (char)(dig + 0x30); d++;
      was_lead = 1;
    }
    n -= dig * mask;
    mask /= 10;
  }
  *d = ';'; d++;
  return (int)(d - dest);
}

static int dps_JSONprint(char *dest, dpsunicode_t n) {
  dpsunicode_t mask = 0x1000;
  register dpsunicode_t dig;
  char *d = dest;
  *d = '\\'; d++;
  switch(n) {
  case 0x08: *d++ = 'b'; break;
  case 0x09: *d++ = 't'; break;
  case 0x0A: *d++ = 'n'; break;
  case 0x0C: *d++ = 'f'; break;
  case 0x0D: *d++ = 'r'; break;
  case 0x22: *d++ = '"'; break;
  case 0x2F: *d++ = '/'; break;
  case 0x5C: *d++ = '\\'; break;
  default:
    *d++ = 'u';
    while(mask > 0) {
      dig = n / mask;
      *d = (char)(dig + ((dig < 10) ? 0x30 : 0x36)); d++;
      n -= dig * mask;
      mask /= 16;
    }
  }
  return (int)(d - dest);
}

#ifdef DEBUG_CONV
int __DPSCALL _DpsConv(DPS_CONV *c, char *d, size_t dlen, const char *s, size_t slen, const char *filename, int line) {
#else
int __DPSCALL DpsConv(DPS_CONV *c, char *d, size_t dlen, const char *s, size_t slen) {
#endif
  size_t	i, codes;
  int           res;
  dpsunicode_t  wc[32]; /* Is 32 enough? */
  dpsunicode_t  zero = 0;
  char		*d_o=d;
  const char	*s_e=s+slen;
  char		*d_e=d+dlen;
  const char	*s_o=s;

#ifdef DEBUG_CONV
  fprintf(stderr, "conv. @ %s:%d from %s (size:%d:json:%d) to %s (size:%d:json:%d)::%s\n", filename, line, c->from->name, slen, c->flags & DPS_RECODE_JSON_FROM, c->to->name, dlen, c->flags & DPS_RECODE_JSON_TO, s); 
#endif
  
  c->istate = 0; /* set default state */
  c->ostate = 0; /* set default state */
  while(s<s_e && d<d_e){
    
    res=c->from->mb_wc(c, c->from, wc,
                       (const unsigned char*)s,
                       (const unsigned char*)s_e);
    if (res > 0) {
      s+=res;
    } else if ((res==DPS_CHARSET_ILSEQ) || (res==DPS_CHARSET_ILSEQ2) || (res==DPS_CHARSET_ILSEQ3) || (res==DPS_CHARSET_ILSEQ4) 
	     || (res==DPS_CHARSET_ILSEQ5) || (res==DPS_CHARSET_ILSEQ6)) {

	switch (res) {
	case DPS_CHARSET_ILSEQ6: s++;
	case DPS_CHARSET_ILSEQ5: s++;
	case DPS_CHARSET_ILSEQ4: s++;
	case DPS_CHARSET_ILSEQ3: s++;
	case DPS_CHARSET_ILSEQ2: s++;
	case DPS_CHARSET_ILSEQ:
	default: s++; break;
	}
        wc[0] = '?';
    } else  break;

    codes = c->ocodes;
    for(i = 0; i < codes; i += c->icodes) {
outp:
      if (wc[i] == 0) goto outaway;
      res = c->to->wc_mb(c, c->to, &wc[i], (unsigned char*)d, (unsigned char*)d_e);
      if (res > 0) {
	d += res;
      } else if (res == DPS_CHARSET_ILUNI && wc[i] != '?') {
	if (c->flags & DPS_RECODE_HTML_TO) {
	  if (d_e-d > 11) {
/*	    res = sprintf(d, "&#%d;", (wc[i] & 0xFFFFFF));*/
	    res = dps_ENTITYprint(d, '&', (wc[i] & 0xFFFFFF));
	    d += res;
	  }
	  else
	    break;
	} else if (c->flags & DPS_RECODE_URL_TO) {
	  if (d_e-d > 11) {
/*	    res = sprintf(d, "!#%d;", (wc[i] & 0xFFFFFF));*/
	    res = dps_ENTITYprint(d, '!', (wc[i] & 0xFFFFFF));
	    d += res;
	  }
	  else
	    break;
	} else if (c->flags & DPS_RECODE_JSON_TO) {
	  if (d_e-d > 6) {
	    res = dps_JSONprint(d, (wc[i] & 0xFFFF));
	    d += res;
	  }
	  else
	    break;
	} else {
	  wc[i] = '?';
	  goto outp;
	}
      } else
	goto outaway;
    }
  }

outaway:  
  if(d <= d_e) {
    res = c->to->wc_mb(c, c->to, &zero, (unsigned char*)d, (unsigned char*)d_e);
  }
  c->ibytes=s-s_o;
  return (c->obytes = d - d_o );
}
 
 
 
int __DPSCALL DpsNConv(DPS_CONV *c, size_t n, char *d, size_t dlen, const char *s, size_t slen) {
  size_t	i, codes, nw = 0;
  int           res;
  dpsunicode_t  wc[32]; /* Is 32 enough? */
  dpsunicode_t  zero = 0;
  char		*d_o = d;
  char          *p_space = d, *p_last = d;
  const char	*s_e = s + slen;
  char		*d_e = d + dlen;
  const char	*s_o = s;

  c->istate = 0; /* set default state */
  c->ostate = 0; /* set default state */
  while(s < s_e && d < d_e) {
    
    res = c->from->mb_wc(c, c->from, wc, (const unsigned char*)s, (const unsigned char*)s_e);
    if (res > 0) {
      s += res;
    } else if ((res==DPS_CHARSET_ILSEQ) || (res==DPS_CHARSET_ILSEQ2) || (res==DPS_CHARSET_ILSEQ3) || (res==DPS_CHARSET_ILSEQ4) 
	     || (res==DPS_CHARSET_ILSEQ5) || (res==DPS_CHARSET_ILSEQ6)) {

	switch (res) {
	case DPS_CHARSET_ILSEQ6: s++;
	case DPS_CHARSET_ILSEQ5: s++;
	case DPS_CHARSET_ILSEQ4: s++;
	case DPS_CHARSET_ILSEQ3: s++;
	case DPS_CHARSET_ILSEQ2: s++;
	case DPS_CHARSET_ILSEQ:
	default: s++; break;
	}
        wc[0] = '?';
    } else  break;

    codes = c->ocodes;
    for(i = 0; i < codes; i += c->icodes) {
outp:
      if (DpsUniNSpace(wc[i]) == 0 || DPS_UNI_CTYPECLASS(DpsUniCType(wc[i])) == DPS_UNI_RAZDEL) {
	p_space = d;
      }
      p_last = d;
      if (wc[i] == 0 || ++nw > n) goto outaway;
      res = c->to->wc_mb(c, c->to, &wc[i], (unsigned char*)d, (unsigned char*)d_e);
      if (res > 0) {
	d += res;
      } else if (res == DPS_CHARSET_ILUNI && wc[i] != '?') {
	if (c->flags & DPS_RECODE_HTML_TO) {
	  if (d_e-d > 11) {
	    res = dps_ENTITYprint(d, '&', (wc[i] & 0xFFFFFF));
	    d += res;
	  }
	  else
	    break;
	} else if (c->flags & DPS_RECODE_URL_TO) {
	  if (d_e-d > 11) {
	    res = dps_ENTITYprint(d, '!', (wc[i] & 0xFFFFFF));
	    d += res;
	  }
	  else
	    break;
	} else if (c->flags & DPS_RECODE_JSON_TO) {
	  if (d_e-d > 6) {
	    res = dps_JSONprint(d, (wc[i] & 0xFFFF));
	    d += res;
	  }
	  else
	    break;
	} else {
	  wc[i] = '?';
	  goto outp;
	}
      } else
	goto outaway;
    }
  }

outaway:
  if (nw > n && p_space < p_last && p_space != d_o) {
    bzero(p_space, (p_last - p_space) * sizeof(char));
    d = p_space;
  }
  if(d <= d_e) {
    res = c->to->wc_mb(c, c->to, &zero, (unsigned char*)d, (unsigned char*)d_e);
  }
  c->ibytes=s-s_o;
  return (c->obytes = d - d_o );
}


extern size_t DpsUniConvLength(DPS_CONV *c, const char *s) {
  dpsunicode_t  wc[32]; /* Are 32 is enough? */
  size_t slen = dps_strlen(s), res_len = 0;
  const char *s_e = s + slen;
  int           res;

  c->istate = 0; /* set default state */
  c->ostate = 0; /* set default state */
  while (s < s_e) {
    res = c->from->mb_wc(c, c->from, wc, (const unsigned char*)s, (const unsigned char*)s_e);
    if (res > 0) {
      s += res;
    } else if ((res==DPS_CHARSET_ILSEQ) || (res==DPS_CHARSET_ILSEQ2) || (res==DPS_CHARSET_ILSEQ3) || (res==DPS_CHARSET_ILSEQ4) 
	     || (res==DPS_CHARSET_ILSEQ5) || (res==DPS_CHARSET_ILSEQ6)) {

	switch (res) {
	case DPS_CHARSET_ILSEQ6: s++;
	case DPS_CHARSET_ILSEQ5: s++;
	case DPS_CHARSET_ILSEQ4: s++;
	case DPS_CHARSET_ILSEQ3: s++;
	case DPS_CHARSET_ILSEQ2: s++;
	case DPS_CHARSET_ILSEQ:
	default: s++; break;
	}
        wc[0] = '?';
    } else  break;
    res_len += c->ocodes;
  }
  return res_len;
}



__C_LINK const char * __DPSCALL DpsCsGroup(const DPS_CHARSET *cs) {
     switch(cs->family){
          case DPS_CHARSET_ARABIC       :    return "Arabic";
          case DPS_CHARSET_ARMENIAN     :    return "Armenian";
          case DPS_CHARSET_BALTIC       :    return "Baltic";
          case DPS_CHARSET_CELTIC       :    return "Celtic";
          case DPS_CHARSET_CENTRAL:          return "Central European";
          case DPS_CHARSET_CHINESE_SIMPLIFIED:    return "Chinese Simplified";
          case DPS_CHARSET_CHINESE_TRADITIONAL:   return "Chinese Traditional";
          case DPS_CHARSET_CYRILLIC     :    return "Cyrillic";
          case DPS_CHARSET_GREEK        :    return "Greek";
          case DPS_CHARSET_HEBREW       :    return "Hebrew";
          case DPS_CHARSET_ICELANDIC    :    return "Icelandic";
          case DPS_CHARSET_JAPANESE     :    return "Japanese";
          case DPS_CHARSET_KOREAN       :    return "Korean";
          case DPS_CHARSET_NORDIC       :    return "Nordic";
          case DPS_CHARSET_SOUTHERN     :    return "South Eur";
          case DPS_CHARSET_THAI         :    return "Thai";
          case DPS_CHARSET_TURKISH      :    return "Turkish";
          case DPS_CHARSET_UNICODE      :    return "Unicode";
          case DPS_CHARSET_VIETNAMESE   :    return "Vietnamese";
          case DPS_CHARSET_WESTERN      :    return "Western";
          case DPS_CHARSET_GEORGIAN     :    return "Georgian";
          case DPS_CHARSET_INDIAN       :    return "Indian";
          case DPS_CHARSET_LAO          :    return "Lao";
          case DPS_CHARSET_IRANIAN      :    return "Iranian";
          case DPS_CHARSET_TAJIK        :    return "Tajik";
          default                       :    return "Unknown";
     }
}

