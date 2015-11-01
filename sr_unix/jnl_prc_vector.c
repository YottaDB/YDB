/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_pwd.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include <errno.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

GBLREF	uint4	process_id;


void jnl_prc_vector (jnl_process_vector *pv)
{
	unsigned char		*c, *s;
	struct passwd	*pw;
        uid_t           eff_uid;       /* try to see why gets pwuid == nil */
	int		gethostname_res;
	time_t		temp_time;

	memset(pv, 0, sizeof(jnl_process_vector));

	pv->jpv_pid = process_id;
	time(&temp_time);
	pv->jpv_time = temp_time;
	GETHOSTNAME(pv->jpv_node, JPV_LEN_NODE, gethostname_res);

	eff_uid = geteuid();          /* save results of each step */
        errno = 0;                    /* force to known value per man page */
	GETPWUID(eff_uid, pw);
	if (pw)
	  {
	    strncpy(pv->jpv_user, pw->pw_name, JPV_LEN_USER);
	    strncpy(pv->jpv_prcnam, pw->pw_name, JPV_LEN_PRCNAM);
	  }
	else
	  {

#ifdef SMW
	    FPRINTF(stderr,"\n?DEBUG - jnl_prc_vector: pw = (nil), euid = %d, pid = %d, errno = %d\n",
		    eff_uid, process_id, errno);
#endif

#ifdef DEBUG
	    strncpy(pv->jpv_user,"ERROR=",JPV_LEN_USER);
	    if (errno < 1000)               /* protect against overflow */
	      c = i2asc((uchar_ptr_t)pv->jpv_user + 6, errno);  /* past = above */

#endif

	  }

        endpwent();                        /* close passwd file to free channel */

	if ((c = (unsigned char *)TTYNAME(0)) != NULL)
	{
	        /* Get basename for tty */
	    	for (s = c + strlen((char*)c);  s > c;  --s)
		    	if (*s == '/')
			{
			    	++s;
				break;
			}

		strncpy(pv->jpv_terminal, (char *)s, JPV_LEN_TERMINAL);
	}
}
