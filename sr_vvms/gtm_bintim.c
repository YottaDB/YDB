/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#include "mdef.h"

#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <jpidef.h>
#include <fab.h>
#include <rab.h>
#include <nam.h>
#include <rmsdef.h>
#include <math.h>	/* for SECOND2EPOCH_SECOND macro */

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"	/* jnl_proc_time needs this. jnl.h needs some of the above */
#include "gtm_bintim.h"

int gtm_bintim(char *toscan, jnl_proc_time *timep)
{
	uint4		status;
	$DESCRIPTOR	(qual_dsc, toscan);
	int		len = strlen(toscan);
	jnl_proc_time	jptime;

	jptime = *timep;
	/* remove quotes */
	while (toscan[len - 1] == '"')
		--len;
	while ('"' == *qual_dsc.dsc$a_pointer)
	{
		--len;
		++qual_dsc.dsc$a_pointer;
	}
	qual_dsc.dsc$w_length = len;
	status = sys$bintim(&qual_dsc,&jptime);
	if (0 > jptime)
	{	/* delta time will be a negative value */
		jptime = MID_TIME(-(jptime));	/* convert to JNL_SHORT_TIME format */
		jptime = -(jptime);
	} else
		jptime = MID_TIME(jptime);	/* convert to JNL_SHORT_TIME format */

	*timep = jptime;
	if (SS$_NORMAL == status)
		return 0;
	else
		return -1;
}
