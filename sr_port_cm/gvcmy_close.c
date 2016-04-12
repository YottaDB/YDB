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

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlkdef.h"
#include "gvcmy_close.h"

void gvcmy_close(struct CLB *c)
{
	unsigned char	msg;
	int		len;
	mlk_pvtblk	*temp, *temp1;
	link_info	*li;

	li = c->usr;
	if (!li->lnk_active)
		return;
	if (li->netlocks)
	{
		temp = li->netlocks;
		while(temp)
		{
			temp1 = temp->next;
			free(temp);
			temp = temp1;
		}
		li->netlocks = 0;
	}
	msg = CMMS_S_TERMINATE;
	c->mbf = &msg;
	c->cbl = SIZEOF(msg);
	c->ast = 0;	/* forces synchronous operation (sys$qiow) */
	/* flushing the buffer is good, but errors are ignored as close is more important and looping on errors is not good */
	cmi_write(c);
	/* Free these structures first because cmi_close frees the structure to which its argument points.  */
	free(c->usr);
	VMS_ONLY(free(c->nod.dsc$a_pointer));
	VMS_ONLY(free(c->tnd.dsc$a_pointer));
	UNIX_ONLY(free(c->nod.addr));
	UNIX_ONLY(free(c->tnd.addr));
	cmi_close(c);
	UNIX_ONLY(cmi_free_clb(c)); /* see comment in cmi_close about freeing clb */
}
