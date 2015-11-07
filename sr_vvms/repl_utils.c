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

#include <ssdef.h>
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <descrip.h>
#include <secdef.h>
#include <efndef.h>

#include "gtm_inet.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and muprec.h */
#include "tp.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "gtmrecv.h"
#include "cli.h"
#include "error.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_shutdcode.h"
#include "repl_sp.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF 	mur_gbls_t	murgbl;

uint4 get_proc_name(unsigned char *prefix, uint4 prefix_size, uint4 pid, unsigned char *buff)
{
	unsigned char *cp;
	int j, n, nbcd;

	cp = buff;
	memcpy(cp, prefix, prefix_size);
	cp    	+= prefix_size;
	nbcd	 = SIZEOF(pid) * 2;
	for (j = 0 ; j < nbcd; j++, pid >>= 4)
	{
		n = pid & 0xf;
		cp[nbcd - 1 - j] = n + (n < 10 ? 48 : 55);
	}
	cp	+= nbcd;
	*cp	 = '\0';
	assert(cp - buff <= PROC_NAME_MAXLEN);
	return (cp - buff);
}

int4 *parse_filename(struct dsc$descriptor_s *d_file, struct dsc$descriptor_s *d_exp_file, boolean_t exp_concealed)
{
	int4		status;
	struct FAB	fab;
	struct NAM	nam;

	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &(nam);
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = STR_OF_DSC(*d_file);
	fab.fab$b_fns = LEN_OF_DSC(*d_file);
	nam.nam$l_esa = STR_OF_DSC(*d_exp_file);
	nam.nam$b_ess = LEN_OF_DSC(*d_exp_file);
	nam.nam$b_nop = (exp_concealed * NAM$M_NOCONCEAL) | NAM$M_SYNCHK;
	if (RMS$_NORMAL == (status = sys$parse(&fab,0,0)))
		if (nam.nam$b_name != 0)
			LEN_OF_DSC(*d_exp_file) = nam.nam$b_esl;
		else
			LEN_OF_DSC(*d_exp_file) = nam.nam$b_esl - nam.nam$b_type - nam.nam$b_ver;
	return status;
}
