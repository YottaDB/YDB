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
#include <fab.h>
#include <rab.h>
#include <descrip.h>
#include <climsgdef.h>
#include "cce_output.h"

static struct RAB *cce_output_rab = 0;
static struct FAB *cce_output_fab = 0;

void cce_out_open(void)
{
	uint4 status;
	static readonly unsigned char sys_output_name[] = "SYS$OUTPUT";
	$DESCRIPTOR(output_qualifier, "OUTPUT");
	char output_name[255];
	$DESCRIPTOR(output_name_desc, output_name);
	short unsigned outnamlen;

	status = cli$get_value(&output_qualifier, &output_name_desc, &outnamlen);
	if (status != 1)
	{
		outnamlen = SIZEOF(sys_output_name) - 1;
		output_name_desc.dsc$a_pointer = sys_output_name;
	}
	cce_output_fab = malloc(SIZEOF(*cce_output_fab));
	cce_output_rab = malloc(SIZEOF(*cce_output_rab));
	*cce_output_fab  = cc$rms_fab;
	*cce_output_rab  = cc$rms_rab;
	cce_output_rab->rab$l_fab = cce_output_fab;
	cce_output_rab->rab$w_usz = 255;
	cce_output_fab->fab$w_mrs = 255;
	cce_output_fab->fab$b_fac = FAB$M_GET | FAB$M_PUT;
	cce_output_fab->fab$b_rat = FAB$M_CR;
	cce_output_fab->fab$l_fna = output_name_desc.dsc$a_pointer;
	cce_output_fab->fab$b_fns = outnamlen;
	status = sys$create(cce_output_fab, 0, 0);
	if ((status & 1) == 0)
		lib$signal(status);
	status = sys$connect(cce_output_rab, 0, 0);
	if ((status & 1) == 0)
		lib$signal(status);
}

void cce_out_write( unsigned char *addr, unsigned int len)
{
	int status;

	cce_output_rab->rab$l_rbf = addr;
	cce_output_rab->rab$w_rsz = len;
	status = sys$put(cce_output_rab,0 ,0);
	if ((status & 1) == 0)
		lib$signal(status);
	return;
}

void cce_out_close(void)
{
	sys$close(cce_output_fab, 0, 0);
	free(cce_output_fab);
	free(cce_output_rab);
	cce_output_fab = 0;
	cce_output_rab = 0;
	return;
}
