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

#define IN_BUFF_SIZE 256

static struct RAB *util_input_rab = 0;
static struct FAB *util_input_fab = 0;
static char inbuff[IN_BUFF_SIZE];
static char *inptr;

void util_in_open(file_prompt)
struct dsc$descriptor_s *file_prompt;
{
	static readonly unsigned char	sys_input_name[] = "SYS$INPUT";
	short unsigned			innamlen;
	uint4			status;
	char				input_name[255];
	$DESCRIPTOR(input_name_desc, input_name);

	if (file_prompt)
	{	status = cli$get_value(file_prompt, &input_name_desc, &innamlen);
		if (status != 1)
		{
			innamlen = SIZEOF(sys_input_name) - 1;
			input_name_desc.dsc$a_pointer = sys_input_name;
		}
	}else
	{
		innamlen = SIZEOF(sys_input_name) - 1;
		input_name_desc.dsc$a_pointer = sys_input_name;
	}
	inptr = inbuff;
	util_input_fab = malloc(SIZEOF(*util_input_fab));
	util_input_rab = malloc(SIZEOF(*util_input_rab));
	*util_input_fab  = cc$rms_fab;
	*util_input_rab  = cc$rms_rab;
	util_input_rab->rab$l_fab = util_input_fab;
	util_input_rab->rab$w_usz = 255;
	util_input_fab->fab$w_mrs = 255;
	util_input_fab->fab$b_fac = FAB$M_GET | FAB$M_PUT;
	util_input_fab->fab$l_fna = input_name_desc.dsc$a_pointer;
	util_input_fab->fab$b_fns = innamlen;
	status = sys$open(util_input_fab, 0, 0);
	if ((status & 1) == 0)
		lib$signal(status);
	status = sys$connect(util_input_rab, 0, 0);
	if ((status & 1) == 0)
		lib$signal(status);
}

char *util_in_read(len)
int *len;
{
	int	status;

	util_input_rab->rab$l_ubf = inbuff;
	status = sys$get(util_input_rab,0 ,0);
	if ((status & 1) == 0)
		lib$signal(status);
	*len = util_input_rab->rab$w_rsz;
	return util_input_rab->rab$l_rbf;
}
