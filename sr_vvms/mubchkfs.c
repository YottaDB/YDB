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
#include <rms.h>
#include <ssdef.h>
#include <descrip>
#include <dvidef>
#include <dcdef>
#include "util.h"
#include "gtmmsg.h"

#define DOTINC ".INC"
#define DOTDAT ".DAT"

GBLREF bool incremental;
GBLDEF bool mubtomag=FALSE;

mstr *mubchkfs (mstr *file)
{
	unsigned char esa[MAX_FN_LEN];
	uint4 status, devclass;
	struct FAB fab;
	struct NAM nam;
	mstr *ret;
	$DESCRIPTOR(dir,"");
	int4 item_code;

	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &(nam);
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = file->addr;
	fab.fab$b_fns = file->len;
	if (incremental)
	{	fab.fab$l_dna = DOTINC;
		fab.fab$b_dns = SIZEOF(DOTINC) - 1;
	}else
	{	fab.fab$l_dna = DOTDAT;
		fab.fab$b_dns = SIZEOF(DOTDAT) - 1;
	}
	nam.nam$l_esa = esa;
	nam.nam$b_ess = MAX_FN_LEN;
	nam.nam$b_nop = NAM$M_SYNCHK;
	if ((status = sys$parse(&fab,0,0)) != RMS$_NORMAL)
	{	gtm_putmsg(VARLSTCNT(1) status);
		return NULL;
	}else
	{	item_code = DVI$_DEVCLASS;
		dir.dsc$a_pointer = nam.nam$l_esa;
		dir.dsc$w_length =  nam.nam$b_esl;
		if ((status = lib$getdvi(&item_code, 0, &dir, &devclass, 0, 0)) != SS$_NORMAL)
		{	gtm_putmsg(status);
			return NULL;
		}
		if (devclass == DC$_TAPE)
		{	if (!incremental)
			{	util_out_print("MUPIP cannot backup to a magnetic tape",TRUE);
				return NULL;
			}else
			{	mubtomag = TRUE;
			}
		}
		ret = malloc(SIZEOF(mstr));
		if (nam.nam$b_name != 0)
			ret->len = nam.nam$b_esl;
		else
			ret->len = nam.nam$b_esl - nam.nam$b_type - nam.nam$b_ver;
		ret->addr = malloc(ret->len + 1);
		memcpy(ret->addr,nam.nam$l_esa,ret->len);
		*(ret->addr + ret->len) = 0;
	}
	return ret;
}
