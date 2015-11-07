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
#include "gdsroot.h"
#include "ccp.h"
#include <dvidef.h>
#include <descrip.h>

GBLDEF unsigned short cce_return_channel;
static struct dsc$descriptor_s cce_return_channel_name;
static unsigned char cce_return_channel_name_buffer[20];

void cce_get_return_channel(p)
ccp_action_aux_value *p;
{
	int status;
	int4 namlen;

	if (!cce_return_channel)
	{
		status = sys$crembx(0, &cce_return_channel, 0,0,0,0, 0);
		if ((status & 1) == 0)
			lib$signal(status);
		cce_return_channel_name.dsc$w_length = SIZEOF(cce_return_channel_name_buffer);
		cce_return_channel_name.dsc$b_dtype = DSC$K_DTYPE_T;
		cce_return_channel_name.dsc$b_class = DSC$K_CLASS_S;
		cce_return_channel_name.dsc$a_pointer = cce_return_channel_name_buffer;
		status = lib$getdvi(&DVI$_FULLDEVNAM, &cce_return_channel,0,0,&cce_return_channel_name, &namlen);
		if ((status & 1) == 0)
			lib$signal(status);
		if (cce_return_channel_name.dsc$w_length > SIZEOF(p->str.txt))
			GTMASSERT;
		cce_return_channel_name.dsc$w_length = namlen;
	}
	p->str.len = cce_return_channel_name.dsc$w_length;
	memcpy(p->str.txt, cce_return_channel_name.dsc$a_pointer, p->str.len);
	return;
}
