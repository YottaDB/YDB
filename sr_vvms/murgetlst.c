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

#include "gtm_string.h"

#include <climsgdef.h>
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "murest.h"
#include <descrip.h>

GBLREF inc_list_struct in_files;

void murgetlst(void)
{
static readonly $DESCRIPTOR(inc_ent,"INPUT_FILE");
unsigned char buffer[MAX_FN_LEN + 1];
$DESCRIPTOR(rn_buf,buffer);
inc_list_struct *ptr;
unsigned short ret_len;

ptr = &in_files;
for (rn_buf.dsc$w_length = MAX_FN_LEN + 1; CLI$GET_VALUE(&inc_ent, &rn_buf, &ret_len) != CLI$_ABSENT;
		rn_buf.dsc$w_length = MAX_FN_LEN + 1)
{
	rn_buf.dsc$w_length = ret_len;
	ptr->next = malloc(SIZEOF(inc_list_struct));
	ptr = ptr->next;
	ptr->next = 0;
	ptr->input_file.len = ret_len;
	ptr->input_file.addr = malloc(ret_len);
	memcpy(ptr->input_file.addr, buffer, ret_len);
}
return;
}
