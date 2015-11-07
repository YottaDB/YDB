/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <jpidef.h>
#include <syidef.h>
#include <ssdef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "vmsdtype.h"


void	jnl_prc_vector(pv)
jnl_process_vector	*pv;
{
	struct
	{
		item_list_3	item[7];
		int4		terminator;
	}		item_list;
	unsigned short	iosb[4];
	uint4	mode, status, dummy;


	memset(pv, 0, SIZEOF(jnl_process_vector));

	sys$gettim(&pv->jpv_time);

	item_list.item[0].buffer_length		= SIZEOF(pv->jpv_pid);
	item_list.item[0].item_code		= JPI$_PID;
	item_list.item[0].buffer_address	= &pv->jpv_pid;
	item_list.item[0].return_length_address	= &dummy;

	item_list.item[1].buffer_length		= SIZEOF(pv->jpv_login_time);
	item_list.item[1].item_code		= JPI$_LOGINTIM;
	item_list.item[1].buffer_address	= &pv->jpv_login_time;
	item_list.item[1].return_length_address	= &dummy;

	item_list.item[2].buffer_length		= SIZEOF(pv->jpv_image_count);
	item_list.item[2].item_code		= JPI$_IMAGECOUNT;
	item_list.item[2].buffer_address	= &pv->jpv_image_count;
	item_list.item[2].return_length_address	= &dummy;

	item_list.item[3].buffer_length		= SIZEOF(mode);
	item_list.item[3].item_code		= JPI$_JOBTYPE;
	item_list.item[3].buffer_address	= &mode;	/* jpv_mode set below */
	item_list.item[3].return_length_address	= &dummy;

	item_list.item[4].buffer_length		= SIZEOF(pv->jpv_user);
	item_list.item[4].item_code		= JPI$_USERNAME;
	item_list.item[4].buffer_address	= pv->jpv_user;
	item_list.item[4].return_length_address	= &dummy;

	item_list.item[5].buffer_length		= SIZEOF(pv->jpv_prcnam);
	item_list.item[5].item_code		= JPI$_PRCNAM;
	item_list.item[5].buffer_address	= pv->jpv_prcnam;
	item_list.item[5].return_length_address	= &dummy;

	item_list.item[6].buffer_length		= SIZEOF(pv->jpv_terminal);
	item_list.item[6].item_code		= JPI$_TERMINAL;
	item_list.item[6].buffer_address	= pv->jpv_terminal;
	item_list.item[6].return_length_address	= &dummy;

	item_list.terminator = 0;

	if ((status = sys$getjpiw(EFN$C_ENF, NULL, NULL, &item_list, iosb, NULL, 0)) != SS$_NORMAL  ||
	    (status = iosb[0]) != SS$_NORMAL)
		rts_error(status);

	pv->jpv_mode = mode;


	item_list.item[0].buffer_length		= SIZEOF(pv->jpv_node);
	item_list.item[0].item_code		= SYI$_NODENAME;
	item_list.item[0].buffer_address	= pv->jpv_node;
	item_list.item[0].return_length_address	= &dummy;

	*((int4 *)&item_list.item[1]) = 0;	/* terminator */

	if ((status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &item_list, iosb, NULL, 0)) != SS$_NORMAL  ||
	    (status = iosb[0]) != SS$_NORMAL)
		rts_error(status);

}
