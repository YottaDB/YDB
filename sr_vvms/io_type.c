/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <ssdef.h>
#include <dcdef.h>
#include <devdef.h>
#include <dvidef.h>
#include "gtm_limits.h"
#include "io.h"
#include <descrip.h>

enum io_dev_type io_type(mstr *tn)
{
	enum io_dev_type type;
	uint4	devchar;	/* device characteristics information */
	uint4	devclass;	/* device classification information */
	uint4	devtype;	/* device type information */
	int4		item_code;
	uint4	stat;
	error_def(ERR_INVSTRLEN);

	$DESCRIPTOR(buf_desc,"");

	if (SHRT_MAX < tn->len)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, tn->len, SHRT_MAX);
	item_code = DVI$_DEVCLASS;
	buf_desc.dsc$a_pointer = tn->addr;
	buf_desc.dsc$w_length = tn->len;
	stat = lib$getdvi(&item_code
			 ,0
			 ,&buf_desc
			 ,&devclass
			 ,0 , 0);
	if (stat == SS$_NOSUCHDEV || stat == SS$_IVDEVNAM)
	{	type = rm;
	}
	else if (stat == SS$_NORMAL)
	{
		item_code = DVI$_DEVCHAR;
		stat = lib$getdvi(&item_code
				 ,0
				 ,&buf_desc
				 ,&devchar
				 ,0 , 0);
		if (stat != SS$_NORMAL &&  stat != SS$_NONLOCAL)
		{	rts_error(VARLSTCNT(1)  stat );
		}
		switch(devclass)
		{
    			case DC$_TAPE:
				if (devchar & DEV$M_FOR)
				{	type = mt;
				}
				else
				{	type = rm;
				}
				break;
			case DC$_TERM:
				type = tt;
				break;
			case DC$_MAILBOX:
				item_code = DVI$_DEVTYPE;
				stat = lib$getdvi(&item_code
						 ,0
						 ,&buf_desc
						 ,&devtype
						 ,0 , 0);
				if (stat != SS$_NORMAL)
				{	rts_error(VARLSTCNT(1)  stat );
				}
				if (devtype == DT$_NULL)
					type = nl;
				else
					type = mb;
				break;
			case DC$_DISK:
			default:
				type = rm;
				break;
		}
	}
	else if (stat == SS$_NONLOCAL)
	{
		type = rm;
	}else
	{	rts_error(VARLSTCNT(1) stat);
	}
	return type;
}
