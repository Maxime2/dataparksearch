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
#include "dps_common.h"
#include "dps_utils.h"
#include "dps_charsetutils.h"
#include "dps_wild.h"
#include "dps_vars.h"
#include "dps_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

#define DPS_VARS_PAS 32


static int varcmp(void *v1, void *v2) {
  register DPS_VAR *uv1 = v1, *uv2 = v2;
  if (uv1->name == NULL) {
    if (uv2->name == NULL) return 0;
    return 1;
  }
  if (uv2->name == NULL) return -1;
  return strcasecmp(uv1->name, uv2->name);
}

void DpsVarFree(DPS_VAR *S){
	DPS_FREE(S->name);
	DPS_FREE(S->val);
	DPS_FREE(S->txt_val);
}

static int DpsVarCopy(DPS_VAR *D, DPS_VAR *S) {
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	if (S->section != 0) D->section = S->section;
	if (S->maxlen != 0) D->maxlen = S->maxlen;
	D->strict = S->strict;
	if (D->single == 0) D->single = S->single;
	D->curlen = S->curlen;
	D->name = (char*)DpsStrdup(S->name);
	if (S->maxlen == 0) {
	  D->val = S->val ? (char*)DpsStrdup(S->val) : NULL;
	  D->txt_val = S->txt_val ? (char*)DpsStrdup(S->txt_val) : NULL;
	} else {
	  size_t len = dps_max(S->maxlen, S->curlen);
	  if (S->val == NULL) {
	    D->val = NULL;
	  } else {
	    D->val = (char*)DpsMalloc(len + 4);
	    if (D->val == NULL) {
#ifdef WITH_PARANOIA
	      DpsViolationExit(-1, paran);
#endif
	      return DPS_ERROR;
	    }
	    dps_strncpy(D->val, S->val, len + 1);
	    D->val[len] = '\0';
	  }
	  if (S->txt_val == NULL) {
	    D->txt_val = NULL;
	  } else {
	    D->txt_val = (char*)DpsMalloc(len + 4);
	    if (D->txt_val == NULL) {
#ifdef WITH_PARANOIA
	      DpsViolationExit(-1, paran);
#endif
	      return DPS_ERROR;
	    }
	    dps_strncpy(D->txt_val, S->txt_val, len + 1);
	    D->txt_val[len] = '\0';
	  }
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}

static int DpsVarCopyNamed(DPS_VAR *D, DPS_VAR *S, const char *name) {
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	if (S->section) D->section = S->section;
	if (S->maxlen) D->maxlen = S->maxlen;
	D->curlen = S->curlen;
	D->strict = S->strict;
	if (D->single == 0) D->single = S->single;
	if(name){
		size_t len = dps_strlen(name) + dps_strlen(S->name) + 3;
		D->name = (char*)DpsMalloc(len);
		if (D->name == NULL) {
#ifdef WITH_PARANOIA
		  DpsViolationExit(-1, paran);
#endif
		  return DPS_ERROR;
		}
		dps_snprintf(D->name, len, "%s.%s", name, S->name);
	}else{
		D->name = (char*)DpsStrdup(S->name);
	}
	if (S->maxlen == 0) {
	  D->val = S->val ? (char*)DpsStrdup(S->val) : NULL;
	  D->txt_val = S->txt_val ? (char*)DpsStrdup(S->txt_val) : NULL;
	} else {
	  size_t len = dps_max(S->maxlen, S->curlen);

	  if (S->val == NULL) {
	    D->val = NULL;
	  } else {
	    D->val = (char*)DpsMalloc(len + 4);
	    if (D->val == NULL) {
#ifdef WITH_PARANOIA
	      DpsViolationExit(-1, paran);
#endif
	      return DPS_ERROR;
	    }
	    dps_strncpy(D->val, S->val, len + 1);
	    D->val[len] = '\0';
	  }
	  if (S->txt_val == NULL) {
	    D->txt_val = NULL;
	  } else {
	    D->txt_val = (char*)DpsMalloc(len + 4);
	    if (D->txt_val == NULL) {
#ifdef WITH_PARANOIA
	      DpsViolationExit(-1, paran);
#endif
	      return DPS_ERROR;
	    }
	    dps_strncpy(D->txt_val, S->txt_val, len + 1);
	    D->txt_val[len] = '\0';
	  }
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}

static void DpsVarSortForLast(DPS_VAR *Var, size_t n) {
  register size_t l = 0, c, r;
  DPS_VAR T = Var[ r = (n - 1) ];
  while (l < r) {
    c = (l + r) / 2;
    if ( varcmp(&Var[c], &T) < 0) l = c + 1;
    else r = c;
  }
  if (r < (n - 1) && varcmp(&Var[r], &T) < 0) r++;
  if (r == (n - 1)) return;
  dps_memmove(&Var[r + 1], &Var[r], (n - r - 1) * sizeof(DPS_VAR));
  Var[r] = T;
  return;
}

int DpsVarListAdd(DPS_VARLIST * Lst, DPS_VAR * S) {
  unsigned int r;

  r = dps_tolower((int)S->name[0]) & 0xFF;
        if (Lst->Root[r].nvars + 1 > Lst->Root[r].mvars) {
	  Lst->Root[r].mvars += DPS_VARS_PAS;
	  Lst->Root[r].Var = (DPS_VAR*)DpsRealloc(Lst->Root[r].Var, (Lst->Root[r].mvars) * sizeof(*Lst->Root[r].Var));
	  if (Lst->Root[r].Var == NULL) {
	    Lst->Root[r].mvars = Lst->Root[r].nvars = 0;
	    return DPS_ERROR;
	  }
	}
	bzero(&Lst->Root[r].Var[Lst->Root[r].nvars], sizeof (DPS_VAR));
	DpsVarCopy(&Lst->Root[r].Var[Lst->Root[r].nvars], S);
	Lst->Root[r].nvars++;
	if(Lst->Root[r].nvars > 1) DpsVarSortForLast(Lst->Root[r].Var, Lst->Root[r].nvars);
	return DPS_OK;
}

static int DpsVarListAddNamed(DPS_VARLIST *Lst, DPS_VAR *S, const char *name) {
	DPS_VAR	v;

	bzero(&v, sizeof(v));
	DpsVarCopyNamed(&v, S, name);
	DpsVarListAdd(Lst, &v);
	DpsVarFree(&v);
	return DPS_OK;
}

DPS_VAR * DpsVarListFind(DPS_VARLIST * vars, const char * name) {
	DPS_VAR key;
	size_t r = (size_t)dps_tolower(*name) & 0xFF;

/*	fprintf(stderr, "VarListFind: %s\n", name);*/
	
	if (!vars->Root[r].nvars) return NULL;
	bzero(&key, sizeof(key));
	key.name = (char *)name;
	return (DPS_VAR*) dps_bsearch(&key, vars->Root[r].Var, vars->Root[r].nvars, sizeof(DPS_VAR), (qsort_cmp)varcmp);
}

int DpsVarListDel(DPS_VARLIST *vars, const char *name) {
	DPS_VAR	*v = DpsVarListFind(vars, name);
	size_t r = (size_t)dps_tolower(*name) & 0xFF;
	size_t nvars; /* = vars->Root[r].nvars - (v - vars->Root[r].Var) - 1;*/
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif

	while (v != NULL) {
	  nvars = vars->Root[r].nvars - (v - vars->Root[r].Var) - 1;
	  DpsVarFree(v);
	  if (nvars > 0) dps_memmove(v, &v[1], nvars * sizeof(*v));
	  vars->Root[r].nvars--;
	  v = DpsVarListFind(vars, name);
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}

int __DPSCALL DpsVarListReplace(DPS_VARLIST * Lst, DPS_VAR * S) {
  if (S != NULL) {
	DPS_VAR	*v = DpsVarListFind(Lst, S->name);
	if (v == NULL) {
		return DpsVarListAdd(Lst, S);
	}else{
		DpsVarFree(v);
		DpsVarCopy(v,S);
	}
  }
  return DPS_OK;
}

static int DpsVarListReplaceNamed(DPS_VARLIST *Lst, DPS_VAR *S, const char *name) {
	int	rc;
	DPS_VAR	*v = DpsVarListFind(Lst, S->name);
	if (v) {
		DpsVarFree(v);
		rc = DpsVarCopyNamed(v, S, name);
	} else {
	  rc = DpsVarListAddNamed(Lst, S, name);
	}
	return rc;
}

static int DpsVarListInsNamed(DPS_VARLIST *Lst, DPS_VAR *S, const char *name) {
	int	rc = DPS_OK;
	DPS_VAR	*v = DpsVarListFind(Lst, S->name);
	if (!v){
	  rc = DpsVarListAddNamed(Lst, S, name);
	}
	return rc;
}


char * DpsVarListFindStrTxt(DPS_VARLIST *vars, const char *name, const char *defval) {
	DPS_VAR * var;
	if((var=DpsVarListFind(vars,name)) != NULL)
	  return (var->txt_val != NULL) ? var->txt_val : ((var->val != NULL) ? var->val : (char*)defval);
	else
	  return (char*)defval;
}

char * DpsVarListFindStr(DPS_VARLIST * vars, const char * name, const char * defval){
	DPS_VAR * var;
	if((var=DpsVarListFind(vars,name)) != NULL)
	  return (var->val != NULL) ? var->val : (char*)defval;
	else
	  return (char*)defval;
}

DPS_VARLIST * DpsVarListInit(DPS_VARLIST *l){
	if(l == NULL) {
		l=(DPS_VARLIST*)DpsMalloc(sizeof(*l));
		if (l == NULL) return NULL;
		bzero((void*)l,sizeof(*l));
		l->freeme=1;
	} else {
		bzero((void*)l,sizeof(*l));
	}
	return l;
}


void DpsVarListFree(DPS_VARLIST * vars){
	register size_t i, r;
	for (r = 0; r < 256; r++) {
	  for (i = 0; i < vars->Root[r].nvars; i++) {
		DPS_FREE(vars->Root[r].Var[i].name);
		DPS_FREE(vars->Root[r].Var[i].val);
		DPS_FREE(vars->Root[r].Var[i].txt_val);
	  }
	  DPS_FREE(vars->Root[r].Var);
	  vars->Root[r].nvars = 0;
	  vars->Root[r].mvars = 0;
	}
	if(vars->freeme)
		DPS_FREE(vars);
}

int DpsVarListAddStr(DPS_VARLIST * vars, const char * name, const char * val) {
  size_t r = (size_t)dps_tolower(*name) & 0xFF;
        if (vars->Root[r].nvars + 1 > vars->Root[r].mvars) {
	  vars->Root[r].mvars += DPS_VARS_PAS;
	  vars->Root[r].Var = (DPS_VAR*)DpsRealloc(vars->Root[r].Var, (vars->Root[r].mvars) * sizeof(*vars->Root[r].Var));
	  if (vars->Root[r].Var == NULL) {
	    vars->Root[r].mvars = vars->Root[r].nvars = 0;
	    return DPS_ERROR;
	  }
	}
	vars->Root[r].Var[vars->Root[r].nvars].section=0;
	vars->Root[r].Var[vars->Root[r].nvars].maxlen=0;
	vars->Root[r].Var[vars->Root[r].nvars].strict=0;
	vars->Root[r].Var[vars->Root[r].nvars].single=0;
	vars->Root[r].Var[vars->Root[r].nvars].curlen = (val != NULL) ? dps_strlen(val) : 0;	
	vars->Root[r].Var[vars->Root[r].nvars].name = name ? (char*)DpsStrdup(name) : NULL;
	vars->Root[r].Var[vars->Root[r].nvars].val = val ? (char*)DpsStrdup(val) : NULL;
	vars->Root[r].Var[vars->Root[r].nvars].txt_val = val ? (char*)DpsStrdup(val) : NULL;
	vars->Root[r].nvars++;
	if (vars->Root[r].nvars > 1) DpsVarSortForLast(vars->Root[r].Var, vars->Root[r].nvars);
	  /*qsort(vars->Root[r].Var, vars->Root[r].nvars, sizeof(DPS_VAR), (qsort_cmp)varcmp);*/
	return vars->Root[r].nvars;
}

int DpsVarListAddInt(DPS_VARLIST * vars,const char * name, int val){
	char num[128];
	dps_snprintf(num, 128, "%d", val);
	return DpsVarListAddStr(vars,name,num);
}

int DpsVarListAddUnsigned(DPS_VARLIST * vars,const char * name, dps_uint4 val){
	char num[128];
	dps_snprintf(num, 128, "%u", val);
	return DpsVarListAddStr(vars, name, num);
}

int DpsVarListAddDouble(DPS_VARLIST * vars,const char * name, double val) {
	char num[128];
	dps_snprintf(num, 128, "%f", val);
	return DpsVarListAddStr(vars, name, num);
}

int DpsVarListInsStr(DPS_VARLIST * vars,const char * name,const char * val){
	return DpsVarListFind(vars,name) ? DPS_OK : DpsVarListAddStr(vars,name,val);
}

int DpsVarListInsInt(DPS_VARLIST * vars,const char * name,int val){
	return DpsVarListFind(vars,name) ? DPS_OK : DpsVarListAddInt(vars,name,val);
}


__C_LINK int __DPSCALL DpsVarListReplaceStr(DPS_VARLIST * vars, const char * name, const char * val) {
	DPS_VAR * var;
	size_t r = (size_t)dps_tolower(*name) & 0xFF;
	
	if((var=DpsVarListFind(vars,name)) != NULL) {
		DPS_FREE(var->val);
		DPS_FREE(var->txt_val);
		if (var->maxlen == 0) {
		  var->val=(val != NULL) ? (char*)DpsStrdup(val) : NULL;
		  var->txt_val = (val != NULL) ? (char*)DpsStrdup(val) : NULL;
		} else {
		  size_t len = dps_max(var->maxlen, var->curlen);
		  if (val == NULL) {
		    var->val = NULL;
		  } else {
		    var->val = (char*)DpsMalloc(len + 4);
		    if (var->val == NULL) {
		      return DPS_ERROR;
		    }
		    dps_strncpy(var->val, val, len + 1);
		    var->val[len] = '\0';
		  }
		  if (val == NULL) {
		    var->txt_val = NULL;
		  } else {
		    var->txt_val = (char*)DpsMalloc(len + 4);
		    if (var->txt_val == NULL) {
		      return DPS_ERROR;
		    }
		    dps_strncpy(var->txt_val, val, len + 1);
		    var->txt_val[len] = '\0';
		  }
		}
		var->curlen = (val != NULL) ? dps_strlen(val) : 0;
	}else{
		DpsVarListAddStr(vars,name,val);
	}
	return vars->Root[r].nvars;
}

__C_LINK int __DPSCALL DpsVarListReplaceInt(DPS_VARLIST * vars, const char * name, int val) {
	DPS_VAR * var;
	char num[64];
	size_t r = (size_t)dps_tolower(*name) & 0xFF;

	if((var=DpsVarListFind(vars,name)) != NULL){
	        dps_snprintf(num, 64, "%d", val);
	        DpsVarListReplaceStr(vars, name, num);
	}else{
		DpsVarListAddInt(vars, name, val);
	}
	return vars->Root[r].nvars;
}

int DpsVarListReplaceUnsigned(DPS_VARLIST * vars, const char * name, unsigned val) {
	DPS_VAR * var;
	char num[128];
	size_t r = (size_t)dps_tolower(*name) & 0xFF;

	if((var = DpsVarListFind(vars, name)) != NULL){
	        dps_snprintf(num, 128, "%u", val);
	        DpsVarListReplaceStr(vars, name, num);
	}else{
		DpsVarListAddUnsigned(vars, name, val);
	}
	return vars->Root[r].nvars;
}

int DpsVarListReplaceDouble(DPS_VARLIST * vars, const char * name, double val) {
	DPS_VAR * var;
	char num[128];
	size_t r = (size_t)dps_tolower(*name) & 0xFF;

	if((var = DpsVarListFind(vars, name)) != NULL){
	        dps_snprintf(num, 128, "%lf", val);
	        DpsVarListReplaceStr(vars, name, num);
	}else{
		DpsVarListAddDouble(vars, name, val);
	}
	return vars->Root[r].nvars;
}


DPS_VAR * DpsVarListFindWithValue(DPS_VARLIST * vars, const char * name, const char * val) {
	size_t i;
	size_t r = (size_t)dps_tolower(*name) & 0xFF;
	
	for(i = 0; i < vars->Root[r].nvars; i++)
		if(!strcasecmp(name, vars->Root[r].Var[i].name) && !strcasecmp(val, vars->Root[r].Var[i].val))
			return(&vars->Root[r].Var[i]);
	return(NULL);
}

int DpsVarListFindInt(DPS_VARLIST * vars, const char * name, int defval) {
	DPS_VAR * var;
/*	int val, rc;*/
	if((var=DpsVarListFind(vars,name)) != NULL) {
	  if (var->val == NULL) return defval;
	  return DPS_ATOI(var->val);
/*	  if ((rc = sscanf(var->val, "%i", &val)) == 1) return val;*/
/*	  printf("IntVal rc:%d\n", rc);*/
/*	  return defval;*/
	} else return(defval);
}

unsigned DpsVarListFindUnsigned(DPS_VARLIST * vars, const char * name, unsigned defval){
	DPS_VAR * var;
	if((var=DpsVarListFind(vars,name)) != NULL)
		return((var->val != NULL) ? (unsigned)strtoull(var->val, (char**)NULL, 0) : defval);
	else
		return(defval);
}

double DpsVarListFindDouble(DPS_VARLIST * vars, const char * name, double defval) {
	DPS_VAR * var;
	if((var=DpsVarListFind(vars,name)) != NULL)
		return((var->val != NULL) ? strtod(var->val, (char**)NULL) : defval);
	else
		return(defval);
}

int DpsVarListReplaceLst(DPS_VARLIST *D, DPS_VARLIST *S, const char *name, const char *mask) {
	size_t i, r, r_from, r_to;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	if (name != NULL) {
	  r_from = (size_t)dps_tolower(*name) & 0xFF;
	  r_to = r_from + 1;
	} else {
	  r_from = 0;
	  r_to = 256;
	}
	
	for (r = r_from; r < r_to; r++)
	for (i = 0; i < S->Root[r].nvars; i++) {
		DPS_VAR	*v = &S->Root[r].Var[i];
		if(!DpsWildCaseCmp(v->name, mask)) {
			DpsVarListReplaceNamed(D, v, name);
		}
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}

int DpsVarListAddLst(DPS_VARLIST *D, DPS_VARLIST *S, const char *name, const char *mask) {
	size_t i, r, r_from, r_to;
	
	if (name != NULL) {
	  r_from = (size_t)dps_tolower(*name) & 0xFF;
	  r_to = r_from + 1;
	} else {
	  r_from = 0;
	  r_to = 256;
	}

	for (r = r_from; r < r_to; r++)
	for (i = 0; i < S->Root[r].nvars; i++) {
		DPS_VAR	*v = &S->Root[r].Var[i];
		if(!DpsWildCaseCmp(v->name, mask)) {
			DpsVarListAddNamed(D, v, name);
		}
	}
	return DPS_OK;
}

int DpsVarListInsLst(DPS_VARLIST *D, DPS_VARLIST *S, const char *name, const char *mask) {
	size_t i, r, r_from, r_to;
	
	if (name != NULL) {
	  r_from = (size_t)dps_tolower(*name) & 0xFF;
	  r_to = r_from + 1;
	} else {
	  r_from = 0;
	  r_to = 256;
	}
	
	for (r = r_from; r < r_to; r++)
	for(i = 0; i < S->Root[r].nvars; i++) {
		DPS_VAR	*v = &S->Root[r].Var[i];
		if(!DpsWildCaseCmp(v->name, mask)) {
			DpsVarListInsNamed(D, v, name);
		}
	}
	return DPS_OK;
}

int DpsVarListDelLst(DPS_VARLIST *D, DPS_VARLIST *S, const char *name, const char *mask) {
	size_t i, r, r_from, r_to;
	
	if (name != NULL) {
	  r_from = (size_t)dps_tolower(*name) & 0xFF;
	  r_to = r_from + 1;
	} else {
	  r_from = 0;
	  r_to = 256;
	}
	
	for (r = r_from; r < r_to; r++)
	for(i = 0; i < S->Root[r].nvars; i++) {
		DPS_VAR	*v = &S->Root[r].Var[i];
		if(!DpsWildCaseCmp(v->name, mask)) {
			DpsVarListDel(D, v->name);
		}
	}
	return DPS_OK;
}


/* 
  Add environment variables 
  into a varlist
*/

extern char **environ;

int DpsVarListAddEnviron(DPS_VARLIST *Vars, const char *name){
	char	**e, *val;
	char	*str;
	size_t  lenstr = 1024;
	
	if ((str = (char*)DpsMalloc(1024)) == NULL) return DPS_ERROR;

	for ( e=environ; e[0] ; e++){
		size_t len = dps_strlen(e[0]);
		if (len > lenstr) {
		  if ((str = (char*)DpsRealloc(str, lenstr = len + 64)) == NULL) return DPS_ERROR;
		}
		len = dps_snprintf(str, lenstr - 1, "%s%s%s", name ? name : "", name ? "." : "", e[0]);
		str[len]='\0';
		
		if((val=strchr(str,'='))){
			*val++='\0';
			DpsVarListReplaceStr(Vars,str,val);
		}
	}
	DPS_FREE(str);
	return DPS_OK;
}

int DpsVarListLog(DPS_AGENT *A, DPS_VARLIST *V, int l, const char *pre) {
	size_t h, r;
	if (DpsNeedLog(l)) {
	  for (r = 0; r < 256; r++)
	    for(h=0; h < V->Root[r].nvars; h++) {
		DPS_VAR *v = &V->Root[r].Var[h];
		if (v->section != 0 || v->maxlen != 0) DpsLog(A, l, "%s.%s [%d,%d:%d%d]: %s", pre, v->name, v->section, v->maxlen, v->single, v->strict, DPS_NULL2STR(v->val));
		else DpsLog(A, l, "%s.%s: %s", pre, v->name, DPS_NULL2STR(v->val));
	    }
	}
	return DPS_OK;
}


