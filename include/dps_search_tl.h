#ifndef _DPS_SEARCH_TL_H
#define _DPS_SEARCH_TL_H

/*
 * udm_search_tl.h from UdmSearch
 * (C) 2000 Kir <kir@sever.net>, UdmSearch Developers Team
 */



/* FIXME: should be taken from template somehow */
#define DEFAULT_DT "back"
#define DEFAULT_DP "0"
#define DEFAULT_DX "1"
#define DEFAULT_DM "0"
#define DEFAULT_DD "1"
#define DEFAULT_DY "2000"
#define DEFAULT_DB "01/01/1999"
#define DEFAULT_DE "31/12/2001"

#include <time.h>

struct dps_stl_info_t{
	int type;
	time_t t1;
	time_t t2;
};

/* Function prototypes */

/* converts string representation of time search type (option)
 * to integer
 */
int getSTLType(char *type_str);

/* converts string in the form dd/mm/yyyy to time_t
 */
time_t dmy2time_t(char * time_str);

/* converts day, month, year to time_t
 */
time_t d_m_y2time_t(int d, int m, int y);

#endif /* _DPS_SEARCH_TL_H */
