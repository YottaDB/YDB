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
#include <secdef.h>
#include <descrip.h>
#include <prvdef.h>
#include <prcdef.h>
#include <rms.h>
#include "util.h"
#include "cli.h"

void cce_start(void)
{
	int4 status;
	uint4	prvadr[2], prvprv[2];
	static readonly $DESCRIPTOR(proc,"GT.CX_CONTROL");
	static readonly $DESCRIPTOR(image,"GTM$DIST:CCP.EXE");
	static readonly	uic = 65540; /* uic = [1,4] */
	$DESCRIPTOR(ccp,"");
	unsigned char success[] = "GT.CX Cluster controller started with PID =        ";
	uint4 pid;
	uint4 baspri;
	struct FAB ccp_fab;
	struct NAM ccp_nam;
	unsigned char ccp_namebuf[63];	/* max image name allowable for creprc */

	prvadr[1] = 0;
	prvadr[0] = PRV$M_DETACH | PRV$M_OPER | PRV$M_SYSNAM | PRV$M_SYSLCK | PRV$M_TMPMBX;
	status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
	if (status == SS$_NORMAL)
	{
		baspri = 5;
		cli_get_num("PRIORITY",&baspri);

		ccp_fab = cc$rms_fab;
		ccp_fab.fab$l_fna = image.dsc$a_pointer;
		ccp_fab.fab$b_fns = image.dsc$w_length;
		ccp_fab.fab$l_nam = &ccp_nam;
		ccp_nam = cc$rms_nam;
		ccp_nam.nam$l_esa = ccp_namebuf;
		ccp_nam.nam$b_ess = SIZEOF(ccp_namebuf);
		ccp_nam.nam$b_nop = NAM$M_SYNCHK;
		status = sys$parse (&ccp_fab);
		if (!(status & 1))
		{	lib$signal(status);
			return;
		}
		ccp.dsc$a_pointer = ccp_namebuf;
		ccp.dsc$w_length = ccp_nam.nam$b_esl;

		status = sys$creprc(&pid, &ccp, 0, 0, 0, &prvadr, 0, &proc, baspri, uic, 0, PRC$M_DETACH);
		sys$setprv(FALSE, &prvprv[0], FALSE, 0);
		if (status != SS$_NORMAL)
		{	lib$signal(status);
			return;
		}
		util_out_open(0);
		i2hex(pid, &success[ SIZEOF(success) - 8], 8);
		util_out_write(&success[0], SIZEOF(success));
		util_out_close();
	}
	else
		lib$signal(status);
}

