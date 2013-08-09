/* Copyright (C) 2003-2005 Datapark corp. All rights reserved.
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
#include "dps_log.h"
#include "dps_signals.h"

#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>


int	have_sighup=0;
int	have_sigterm=0;
int	have_sigpipe=0;
int	have_sigint=0;
int	have_sigusr1 = 0;
int	have_sigusr2 = 0;
int     have_sigalrm = 0;



void (*DpsSignal(int signo, void (*handler)(int)))(int) {
  struct sigaction act, oact;

  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags  = 0;
  if (signo == SIGCHLD) act.sa_flags |= SA_NOCLDSTOP;
  act.sa_flags |= SA_RESTART;
  if (sigaction(signo, &act, &oact) < 0) {
    return(SIG_ERR);
  }
  return(oact.sa_handler);
}



#ifdef HAVE_POSIX_SIGNALS

static RETSIGTYPE dps_sig_handler(int signo){
/*	fprintf(stderr, "\nReceived signal %d", signo);*/
	switch(signo){
		case SIGHUP:
			have_sighup=1;
/*			fprintf(stderr, ", SIGHUP.\n");*/
			break;
		case SIGINT:
			have_sigint = 1;
/*			fprintf(stderr, ", SIGINT. Terminating. Please wait...\n");*/
			break;
		case SIGTERM:
			have_sigterm = 1;
/*			fprintf(stderr, ", SIGTERM. Terminating. Please wait...\n");*/
			break;
		case SIGALRM:
			have_sigalrm=1;
/*			fprintf(stderr, ", SIGALRM.\n");*/
			break;
		case SIGUSR1:
			have_sigusr1 = 1;
/*			fprintf(stderr, ", SIGUSR1\n");*/
			break;
		case SIGUSR2:
			have_sigusr2 = 1;
/*			fprintf(stderr, ", SIGUSR2\n");*/
			break;
		default:
			exit(1);
	}
}

int DpsSigHandlersInit(DPS_AGENT *Indexer) {

    struct  sigaction sa, si;
    int ret=0;

    sa.sa_handler=dps_sig_handler;
    sa.sa_flags=0;
    sigemptyset(&sa.sa_mask);
    si.sa_handler = SIG_IGN;
    si.sa_flags = 0;
    sigemptyset(&si.sa_mask);


    if ((ret=sigaction(SIGTERM, &sa, (struct sigaction *)NULL)))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");

    if ((ret=sigaction(SIGHUP, &sa, (struct sigaction *)NULL)))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");

    if ((ret=sigaction(SIGPIPE, &si, (struct sigaction *)NULL)))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");

    if (ret+=sigaction(SIGINT, &sa, (struct sigaction *)NULL))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");

    if (ret+=sigaction(SIGALRM, &sa, (struct sigaction *)NULL))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");

    if ((ret=sigaction(SIGUSR1, &sa, (struct sigaction *)NULL)))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");
    if ((ret=sigaction(SIGUSR2, &sa, (struct sigaction *)NULL)))
	DpsLog(Indexer, DPS_LOG_WARN, "Can't set sighandler");


    return DPS_OK;
}

#else /* We have no POSIX signals :( */

int DpsSigHandlersInit(DPS_AGENT *Indexer) {
    /* FIXME: implement other type of signal routines */
    return(0);
};

#endif /* HAVE_POSIX_SIGNALS */
