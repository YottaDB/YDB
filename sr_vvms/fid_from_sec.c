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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <descrip.h>

#define PREFIX_SIZE 4

fid_from_sec(section,file)
struct dsc$descriptor	*section;
gds_file_id		*file;
{
	int		i, j, k, l = 0;
	char		*cptr, *ctop;

	/* See the routine GLOBAL_NAME for the encoding algorithm.  Keep these two routines in tandom */

	for (cptr = ctop = section->dsc$a_pointer + section->dsc$w_length - 1; *cptr != '$'; cptr--)
			;
	file->dvi[0] = (cptr - section->dsc$a_pointer) - PREFIX_SIZE;
	memcpy (&file->dvi[1], (char *)section->dsc$a_pointer + 4, file->dvi[0]);
	for ( i = (SIZEOF(file->fid) / SIZEOF(file->fid[0])) - 1; i >= 0 ; i--)
	{	for ( j = 0; j < SIZEOF(file->fid[0]) * 2 ; j++, ctop--)
		{	k = *ctop - (*ctop > '9' ? 55 : 48);
			l = (l << 4) + k;
		}
		file->fid[i] = l;
	}
	return;
}
