/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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
#include "dps_socket.h"
#include "dps_host.h"
#include "dps_proto.h"
#include "dps_utils.h"
#include "dps_mutex.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(WITH_IDN) && !defined(APACHE1) && !defined(APACHE2)
#include <idna.h>
#elif defined(WITH_IDNKIT) && !defined(APACHE1) && !defined(APACHE2)
#include <idn/api.h>
#endif


#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#define MAX_HOST_ADDR_SIZE	256

/*
#define DEBUG 1
*/

static int cmphost(const DPS_HOST_ADDR *v1, const DPS_HOST_ADDR *v2) {
  if (v1->hostname == NULL) {
    if (v2->hostname == NULL) return 0;
    return -1;
  }
  if (v2->hostname == NULL) return 1;
  return strcasecmp(v1->hostname, v2->hostname);
}


static int host_addr_add(DPS_AGENT *Indexer, DPS_HOSTLIST *List, const char *hostname, DPS_CONN *connp) {
	int min_id;
	size_t i;
	
	if (hostname == NULL) return 0;
	if((List->nhost_addr>=List->mhost_addr)&&(List->mhost_addr<MAX_HOST_ADDR_SIZE)){
		List->mhost_addr += MAX_HOST_ADDR_SIZE; /* FIXME: was 32 */
		List->host_addr = DpsRealloc(List->host_addr, List->mhost_addr*sizeof(DPS_HOST_ADDR));
		if (List->host_addr == NULL) {
		  List->mhost_addr = List->nhost_addr = 0;
		  return DPS_ERROR;
		}
		bzero((void*)&List->host_addr[List->nhost_addr], (List->mhost_addr-List->nhost_addr)*sizeof(DPS_HOST_ADDR));
	}

	if((List->nhost_addr<List->mhost_addr)&&(List->mhost_addr<=MAX_HOST_ADDR_SIZE)){
		min_id = List->nhost_addr;
		List->nhost_addr++;
	}else{
		int min;

		min = List->host_addr[0].last_used;
		min_id=0;
		/* find last used host */
		for (i=0; i<List->nhost_addr; i++)
			if(List->host_addr[i].last_used < List->host_addr[min_id].last_used){
				min = List->host_addr[i].last_used;
				min_id=i;
			}
	}

	List->host_addr[min_id].last_used = Indexer->now;
/*	if(addr) dps_memcpy(&List->host_addr[min_id].addr, addr, sizeof( struct in_addr));*/
	if (connp) {
	  for(i = 0; i < connp->n_sinaddr; i++)
	    dps_memcpy(&List->host_addr[min_id].addr[i], &connp->sinaddr[i], sizeof( connp->sinaddr[0])); /* was: dps_memmove */
	  List->host_addr[min_id].naddr = connp->n_sinaddr;
	}
	
	DPS_FREE(List->host_addr[min_id].hostname);
	List->host_addr[min_id].hostname = (char*)DpsStrdup(hostname);
	List->host_addr[min_id].net_errors=0;

	DpsPreSort(List->host_addr, List->nhost_addr, sizeof(DPS_HOST_ADDR), (qsort_cmp)cmphost);
	return 0;
}

static DPS_HOST_ADDR *host_addr_find(DPS_HOSTLIST *List, const char *hostname) {
/*	int l,c,r,res;*/
        DPS_HOST_ADDR key;

        /* Find current URL in sorted part of list */

	if (List->nhost_addr == 0 || hostname == NULL) return NULL;
	key.hostname = hostname;
	return dps_bsearch(&key, List->host_addr, List->nhost_addr, sizeof(DPS_HOST_ADDR), (qsort_cmp)cmphost);

/*	

        l=0;r=List->nhost_addr-1;
        while(l<=r){
                c=(l+r)/2;
		if(!(res = strcasecmp(List->host_addr[c].hostname, hostname)))
                        return(&List->host_addr[c]);
                
                if(res<0)
                        l=c+1;
                else
                        r=c-1;
        }
	return NULL;
*/
}

#if defined(HAVE_LIBCARES) && (ARES_VERSION > 0x010500)
static void dps_callback(void *arg, int status, int timeouts, struct hostent *he) {
#else
static void dps_callback(void *arg, int status, struct hostent *he) {
#endif
  DPS_CONN *connp = (DPS_CONN*)arg;
  size_t i = 0;
  if (he != NULL)
    for (i = 0; he->h_addr_list[i] != NULL && i < DPS_MAX_HOST_ADDR; i++) {
      if (he->h_addrtype != AF_INET) continue;
      dps_memcpy((char *)&(connp->sinaddr[i].sin_addr), he->h_addr_list[i], (size_t)he->h_length); /* was: dps_memmove */
      connp->sinaddr[i].sin_port = htons((uint16_t)connp->port); /*	Set port	*/
/*      if (DpsCheckAddr(&(connp->sin), connp->timeout) == DPS_OK) break;*/
    }
  connp->n_sinaddr = i;
}


static int DpsGetHostByName(DPS_AGENT *Indexer, DPS_CONN *connp, const char *hostname) {
  int i;

#if defined(HAVE_LIBARES) || defined(HAVE_LIBCARES)
  int nfds;
  fd_set read_fds, write_fds;
  struct timeval *tvp, tv;

  ares_gethostbyname(Indexer->channel, hostname, AF_INET, dps_callback, connp);
  while (1) {
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    nfds = ares_fds(Indexer->channel, &read_fds, &write_fds);
    if (nfds == 0) break;
    tvp = ares_timeout(Indexer->channel, NULL, &tv);
    select(nfds, &read_fds, &write_fds, NULL, tvp);
    ares_process(Indexer->channel, &read_fds, &write_fds);
  }

#elif defined(HAVE_PTHREAD) && defined(HAVE_GETADDRINFO)
  {
    struct addrinfo hints, *res = NULL, *list;
    int error = EAI_AGAIN;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

#if defined(WITH_TRACE) && defined(DEBUG)
    fprintf(Indexer->TR, "Resolver calling getaddrinfo\n");
    fflush(Indexer->TR);
#endif
    for (i = 0; (i < 5) && (error == EAI_AGAIN); i++) {
#if defined(WITH_TRACE) && defined(DEBUG)
      fprintf(Indexer->TR, "Pas: %d begin\n", i);
      fflush(Indexer->TR);
#endif

      error = getaddrinfo(hostname, NULL, &hints, &res);

#if defined(WITH_TRACE) && defined(DEBUG)
      fprintf(Indexer->TR, "Pas: %d done\n", i);
      fflush(Indexer->TR);
#endif
    }
    if (error == EAI_NONAME) {
      error = EAI_AGAIN;
      DpsLog(Indexer, DPS_LOG_DEBUG, "%s not found, trying original %s", hostname, connp->hostname);
      for (i = 0; (i < 5) && (error == EAI_AGAIN); i++) {
#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "2Pas: %d begin\n", i);
	fflush(Indexer->TR);
#endif

	error = getaddrinfo(connp->hostname, NULL, &hints, &res);

#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "2Pas: %d done\n", i);
	fflush(Indexer->TR);
#endif
      }
    }
    if (error || res == NULL) {
      if (error) {
	DpsLog(Indexer, DPS_LOG_WARN, "%s: getaddrinfo error: %s", hostname, gai_strerror(error));
      } else {
	DpsLog(Indexer, DPS_LOG_WARN, "%s: no adresses", hostname);
      }
      if (res != NULL) freeaddrinfo(res);
      return -6;
    }
#if defined(WITH_TRACE) && defined(DEBUG)
    fprintf(Indexer->TR, "Resolver looking result\nconnp.timeout: %d\n", connp->timeout);
    fflush(Indexer->TR);
#endif
    for(list = res, i = 0; (list != NULL) && (i < DPS_MAX_HOST_ADDR); list = list->ai_next, i++) {
      if (list->ai_family != AF_INET) continue;
      dps_memcpy((char *)&(connp->sinaddr[i]), list->ai_addr, (size_t)list->ai_addrlen); /*was: dps_memmove */
      connp->sinaddr[i].sin_port = htons(connp->port); /*	Set port	*/
      DpsLog(Indexer, DPS_LOG_DEBUG, "Resolver %dth checking for %s", i, inet_ntoa(connp->sinaddr[i].sin_addr));
/*      if ((list->ai_next != NULL) && (DpsCheckAddr(&(connp->sin), connp->timeout) == DPS_OK)) break;*/
    }
    connp->n_sinaddr = i;
/*    DpsLog(Indexer, DPS_LOG_DEBUG, "Resolver: %s - > %s", hostname, inet_ntoa(connp->sin.sin_addr));*/
    if (res != NULL) freeaddrinfo(res);
  }
#elif defined(HAVE_PTHREAD) && (defined(HAVE_FUNC_GETHOSTBYNAME_R_3) || defined(HAVE_FUNC_GETHOSTBYNAME_R_5) || defined(HAVE_FUNC_GETHOSTBYNAME_R_6))
  {
    struct hostent *he = NULL;
    struct hostent hpstr;
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_3
    struct hostent_data hdata;
#endif
    char   buf[BUFSIZ];
    int    herrno = 0;

    for (i = 0; (i < 3) && (he == NULL); i++) {
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_3
      if (gethostbyname_r(hostname, &hpstr, &hdata) == 0) {
	he = &hpstr;
      } else {
	he = NULL;
      }
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
      he = gethostbyname_r(hostname, &hpstr, buf, sizeof(buf), &herrno);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_6)
      gethostbyname_r(hostname, &hpstr, buf, sizeof(buf), &he, &herrno);
#else
#error gethostbyname_r has unknown number of arguments
#endif
    }
    if (he == NULL) {
      connp->err = DPS_NET_CANT_RESOLVE;
      return -7;
    }
    for (i = 0; he->h_addr_list[i] != NULL && i < DPS_MAX_HOST_ADDR; i++) {
      if (he->h_addrtype != AF_INET) continue;
      dps_memcpy((char *)&(connp->sinaddr[i].sin_addr), he->h_addr_list[i], (size_t)he->h_length); /* was: dps_memmove */
      connp->sinaddr[i].sin_port = htons(connp->port); /*	Set port	*/
      DpsLog(Indexer, DPS_LOG_DEBUG, "Resolver %dth checking for %s", i, inet_ntoa(connp->sinaddr[i].sin_addr));
/*      if (DpsCheckAddr(&(connp->sin), connp->timeout) == DPS_OK) break;*/
    }
    connp->n_sinaddr = i;
  }
#else
  {
    struct hostent *he = NULL;
		
    /* 
       Note:
       gethostbyname() isn't reentrant 
    */
		  
    for (i = 0; (i < 3) && (he == NULL); i++) {
      he = gethostbyname(hostname);
    }

    if (he == NULL) {
#if defined(h_errno) && defined(hstrerror)
      DpsLog(Indexer, DPS_LOG_ERROR, "gethostbyname error: %s", hstrerror(h_errno));
#endif
      return -8;
    }
    for (i = 0; he->h_addr_list[i] != NULL && i < DPS_MAX_HOST_ADDR; i++) {
      if (he->h_addrtype != AF_INET) continue;
      dps_memcpy((char *)&(connp->sinaddr[i].sin_addr), he->h_addr_list[i], (size_t)he->h_length); /*was: dps_memmove */
      connp->sinaddr[i].sin_port = htons(connp->port); /*	Set port	*/
      DpsLog(Indexer, DPS_LOG_DEBUG, "Resolver %dth checking for %s", i, inet_ntoa(connp->sinaddr[i].sin_addr));
/*      if (DpsCheckAddr(&(connp->sin), connp->timeout) == DPS_OK) break;*/
    }
    connp->n_sinaddr = i;

  }
#endif
  return 0;
}


static ssize_t Read(int p, void* buf, size_t len) {
  size_t left = len;

  while (left) {
    ssize_t rd;
    rd = read(p, (char*)buf + len - left, left);
    if (rd < 0) break;
    left -= rd;
  }
  return 1;
}


static void DpsResolver(DPS_AGENT *Indexer) {
  DPS_CONN connp;
  char hostname[1024];
  char connp_hostname[1024];
  size_t i, len;
  ssize_t size;
  int rc;

  while(1) {
    size = read(Indexer->snd_pipe[0], &len, sizeof(len));
    if (size > 0) {
      if (len == 0) {
	DpsLog(Indexer, DPS_LOG_EXTRA, "Resolver process %d received terminate command and exited", getpid());
	break;
      }
      Read(Indexer->snd_pipe[0], hostname, len);
      hostname[len] = '\0';
      Read(Indexer->snd_pipe[0], &len, sizeof(len));
      Read(Indexer->snd_pipe[0], connp_hostname, len);
      connp_hostname[len] = '\0';
      connp.hostname = connp_hostname;
      connp.err = 0;
      rc = DpsGetHostByName(Indexer, &connp, hostname);
      if (rc != 0) {
	connp.err = DPS_NET_CANT_RESOLVE;
	(void)write(Indexer->rcv_pipe[1], &connp.err, sizeof(connp.err));
      } else {
	(void)write(Indexer->rcv_pipe[1], &connp.err, sizeof(connp.err));
	(void)write(Indexer->rcv_pipe[1], &connp.n_sinaddr, sizeof(connp.n_sinaddr));
	for(i = 0; i < connp.n_sinaddr; i++) {
	  (void)write(Indexer->rcv_pipe[1], &connp.sinaddr[i], sizeof(connp.sinaddr[0]));
	}
      }

    } else if (size < 0) {
      dps_strerror(Indexer, DPS_LOG_ERROR, "Error pipe reading in resolver process %d, exiting", getpid());
      break;
    }
    DPSSLEEP(0);
  }
}


static void DpsGetResolver(DPS_AGENT *Indexer, DPS_CONN *connp, const char *hostname) {
  size_t i, len = dps_strlen(DPS_NULL2EMPTY(hostname)) + 1;

  (void)write(Indexer->snd_pipe[1], &len, sizeof(len));
  (void)write(Indexer->snd_pipe[1], DPS_NULL2EMPTY(hostname), len);
  len = dps_strlen(DPS_NULL2EMPTY(connp->hostname));
  (void)write(Indexer->snd_pipe[1], &len, sizeof(len));
  (void)write(Indexer->snd_pipe[1], DPS_NULL2EMPTY(connp->hostname), len);

  Read(Indexer->rcv_pipe[0], &connp->err, sizeof(connp->err));
  if (connp->err == 0) {
    Read(Indexer->rcv_pipe[0], &connp->n_sinaddr, sizeof(connp->n_sinaddr));
    for (i = 0; i < connp->n_sinaddr; i++) {
      Read(Indexer->rcv_pipe[0], &connp->sinaddr[i], sizeof(connp->sinaddr[0]));
    }
  }

}



int DpsResolverStart(DPS_AGENT *Indexer) {
  (void)pipe(Indexer->rcv_pipe);
  (void)pipe(Indexer->snd_pipe);

  if ((Indexer->resolver_pid = fork()) == 0) { /* child process */

/*    DpsDestroyMutexes();*/
    DpsInitMutexes();
    Indexer->Conf->is_log_open = 0;
    DpsOpenLog("indexer", Indexer->Conf, log2stderr);

    dps_setproctitle("[%d] hostname resolver", Indexer->handle);

    close(Indexer->rcv_pipe[0]);
    close(Indexer->snd_pipe[1]);
    Indexer->rcv_pipe[0] = Indexer->snd_pipe[1] = -1;

    DpsResolver(Indexer);
    exit(0);

  } else { /* parent process */
    close(Indexer->rcv_pipe[1]);
    close(Indexer->snd_pipe[0]);
    Indexer->rcv_pipe[1] = Indexer->snd_pipe[0] = -1;
  }

  return DPS_OK;
}

int DpsResolverFinish(DPS_AGENT *Indexer) {
  size_t len;
  int status;

  len = 0;
  (void)write(Indexer->snd_pipe[1], &len, sizeof(len));
  waitpid(Indexer->resolver_pid, &status, 0);

  if (Indexer->rcv_pipe[0] >= 0) close(Indexer->rcv_pipe[0]);
  if (Indexer->rcv_pipe[1] >= 0) close(Indexer->rcv_pipe[1]);
  if (Indexer->snd_pipe[0] >= 0) close(Indexer->snd_pipe[0]);
  if (Indexer->snd_pipe[1] >= 0) close(Indexer->snd_pipe[1]);

  return DPS_OK;
}


int DpsHostLookup(DPS_AGENT *Indexer, DPS_CONN *connp) {
	DPS_HOST_ADDR  *Host;
	DPS_HOSTLIST *List = &Indexer->Hosts;
	char   *ascii = NULL;
	int rc = DPS_OK;
	size_t i;
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	DPS_CHARSET *url_cs, *uni_cs;
	DPS_CONV  url_uni;
	char    *uni = NULL;
	size_t len;
#endif
	TRACE_IN(Indexer, "DpsHostLookup");

	if (!connp->hostname || connp->hostname[0] == '\0') {
	        TRACE_OUT(Indexer);
		return -1;
	}
	
#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "Resolver looking for %s\n", connp->hostname);
	fflush(Indexer->TR);
#endif

	bzero((void*)&connp->sin, sizeof(struct sockaddr_in));
	
	/*	Check port	*/
	if (connp->port == 0) {
	  DpsLog(Indexer, DPS_LOG_DEBUG, "Zero port at %s:%d", __FILE__, __LINE__);
		connp->err = DPS_NET_ERROR;
		TRACE_OUT(Indexer);
		return -2;
	}

	/* Check if hostname in standard numbers-and-dots notation */ 
	if ((connp->sin.sin_addr.s_addr = inet_addr(connp->hostname)) == INADDR_NONE) {

		if((Host=host_addr_find(List,connp->hostname))){
			Host->last_used = Indexer->now;
			connp->Host = Host;
			if(Host->naddr) {
			        for (i = 0; i < Host->naddr; i++) {
				  dps_memcpy(&connp->sinaddr[i], &Host->addr[i], sizeof(Host->addr[0])); /*was: dps_memmove */
				}
				connp->n_sinaddr = Host->naddr;
			}else{
				connp->err = DPS_NET_CANT_RESOLVE;
				TRACE_OUT(Indexer);
				return -3;
			}
			/*	Set port	*/
			connp->sin.sin_port = htons(connp->port);
#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "Resolver find: %s - > %s [Host.addr: %s]\n", connp->hostname, inet_ntoa(connp->sin.sin_addr),
		inet_ntoa(connp->Host->addr));
	fflush(Indexer->TR);
#endif
			TRACE_OUT(Indexer);
			return 0;
		}

#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
		if (connp->charset_id != DPS_CHARSET_US_ASCII) {
		  uni_cs = DpsGetCharSet("UTF-8");
		  url_cs = DpsGetCharSetByID(connp->charset_id);
		  DpsConvInit(&url_uni, url_cs, uni_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);

		  uni = (char*)DpsMalloc(48 * (len = dps_strlen(connp->hostname) + 1));
		  if (uni == NULL) {
		    connp->err = DPS_NET_CANT_RESOLVE;
		    TRACE_OUT(Indexer);
		    return -4;
		  }
		  DpsConv(&url_uni, (char*)uni, 48 * len, connp->hostname, len);
#ifdef WITH_IDN
		  if ( (rc = idna_to_ascii_8z((const char*)uni, &ascii, 0)) != IDNA_SUCCESS) {
		    DpsLog(Indexer, DPS_LOG_ERROR, "IDN: %s [%s] -> %s [UTF-8]  rc:%d", connp->hostname, url_cs->name, (char*)uni, rc);
		    DPS_FREE(uni);
		    connp->err = DPS_NET_CANT_RESOLVE;
		    TRACE_OUT(Indexer);
		    return -5;
		  }
#else
		  ascii = (char*)DpsMalloc(len);
		  if (ascii == NULL) {
		    DPS_FREE(uni); 
		    connp->err = DPS_NET_CANT_RESOLVE;
		    TRACE_OUT(Indexer);
		    return -6;
		  }
		  if (idn_encodename(IDN_IDNCONV, (const char *)uni, ascii, len) != idn_success) {
		    DpsLog(Indexer, DPS_LOG_ERROR, "IDN: %s [%s] -> %s [UTF-8]  rc:%d", connp->hostname, url_cs->name, (char*)uni, rc);
		    DPS_FREE(ascii);
		    DPS_FREE(uni); 
		    connp->err = DPS_NET_CANT_RESOLVE;
		    TRACE_OUT(Indexer);
		    return -7;
		  }
#endif
		  DpsLog(Indexer, DPS_LOG_DEBUG, "IDN: %s [%s] -> %s", connp->hostname, url_cs->name, ascii);
		  DPS_FREE(uni);
		}
#else
		ascii = connp->hostname;
#endif

		if (Indexer->resolver_pid != 0) {

		  DpsGetResolver(Indexer, connp, (ascii != NULL) ? ascii : connp->hostname);

		} else {
		  rc = DpsGetHostByName(Indexer, connp, (ascii != NULL) ? ascii : connp->hostname);
		  if (rc != 0) {
		    connp->err = DPS_NET_CANT_RESOLVE;
		  }
		}

		if (connp->err != 0) {
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
		  DPS_FREE(ascii);
#endif
		  TRACE_OUT(Indexer);
		  return rc;
		}

		host_addr_add(Indexer, List, connp->hostname, connp);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
		DPS_FREE(ascii);
#endif
	}else{
	        connp->n_sinaddr = 1;
		dps_memcpy(&connp->sinaddr[0], &connp->sin, sizeof(connp->sinaddr[0])); /* was: dps_memmove */
		if(!(Host=host_addr_find(List,connp->hostname))){
		  host_addr_add(Indexer, List,connp->hostname, connp);
		}
	}
	connp->Host = host_addr_find(List, connp->hostname);
	/*	Set port	*/
	connp->sin.sin_port = htons((uint16_t)connp->port);

#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "Resolver: %s - > %s [Host.addr: %s]\n", connp->hostname, inet_ntoa(connp->sin.sin_addr),
		inet_ntoa(connp->Host->addr));
	fflush(Indexer->TR);
#elif defined(DEBUG)
	{ register int z;
	  for (z=0; z < connp->Host->naddr; z++) 
	    DpsLog(Indexer, DPS_LOG_DEBUG, "Resolver: %s - > %s [Host.addr.%d: %s]\n", connp->hostname, inet_ntoa(connp->sin.sin_addr),
		   z, inet_ntoa(connp->Host->addr[z].sin_addr));
	}
#endif

	TRACE_OUT(Indexer);
	return 0;
}

void DpsHostListFree(DPS_HOSTLIST *List){
	size_t i;
	
	for(i=0; i<List->nhost_addr; i++)
		DPS_FREE(List->host_addr[i].hostname);
	
	DPS_FREE(List->host_addr);
	List->nhost_addr = List->mhost_addr = 0;
	return;
}


void DpsIPListFree(DPS_IPLIST *List){
	
	DPS_FREE(List->ip_addr);
	List->nip_addr = List->mip_addr = 0;
	return;
}


