/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <jpidef.h>
#include <descrip.h>

#include "vmsdtype.h"
#include "job.h"
#include "efn.h"
#include "min_max.h"


GBLREF mval		dollar_job;
GBLDEF char		defprcnambuf[MAX_PRCNAM_LEN];

static short	jobcnt = 0;

void ojdefprcnam (struct dsc$descriptor_s *prcnam)
{
	int4		status;
	char		*t;
	unsigned short	prcnamlen;
	char		username[12];
	unsigned short	ret;
	short		iosb[4];
	struct
	{
		item_list_3	le[1];
		int4		terminator;
	}		item_list;
	$DESCRIPTOR	(blank, " ");
	$DESCRIPTOR	(usrnam, &username[0]);
	unsigned short	usernamelen;
	char		pidstr[8];
	unsigned short	pidstrlen;
	char		jobcntstr[8];
	unsigned short	jobcntstrlen;
	unsigned short	ojhex_to_str ();

	item_list.le[0].buffer_length		= sizeof username;
	item_list.le[0].item_code		= JPI$_USERNAME;
	item_list.le[0].buffer_address		= &username[0];
	item_list.le[0].return_length_address	= &ret;
	item_list.terminator			= 0;

	status = sys$getjpi (0, 0, 0, &item_list, &iosb[0], 0, 0);
	if (!(status & 1))	rts_error(VARLSTCNT(1) status);
	sys$synch (efn_immed_wait, &iosb[0]);
	if (!(iosb[0] & 1))	rts_error(VARLSTCNT(1) iosb[0]);

	usrnam.dsc$w_length = sizeof username;
	if ((usernamelen = lib$locc (&blank, &usrnam)) == 0)
		usernamelen = 13;

	--usernamelen;
	pidstrlen = ojhex_to_str ((int4) MV_FORCE_INTD(&dollar_job), &pidstr[0]);
	jobcnt++;
	jobcntstrlen = ojhex_to_str (jobcnt, &jobcntstr[0]);
	usernamelen = MIN(usernamelen, MAX_PRCNAM_LEN - (1 + pidstrlen + 1 + jobcntstrlen));
	prcnamlen = usernamelen + 1 + pidstrlen + 1 + jobcntstrlen;
	assert (prcnamlen <= MAX_PRCNAM_LEN);
	prcnam->dsc$w_length = prcnamlen;
	t = prcnam->dsc$a_pointer = &defprcnambuf[0];
	memcpy (t, &username[0], usernamelen);
	t += usernamelen;
	*t++ = '_';
	memcpy (t, &pidstr[0], pidstrlen);
	t += pidstrlen;
	*t++ = 'J';
	memcpy (t, &jobcntstr[0], jobcntstrlen);
	return;
}
