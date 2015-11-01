/****************************************************************
 *								*
 *	Copyright 2004, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_ctype.h"

#include "gtm_logicals.h"
#include "trans_numeric.h"
#include "trans_log_name.h"
#include "iosp.h"		/* for SS_ */
#include "nametabtyp.h"		/* for namelook */
#include "namelook.h"
#include "io.h"
#include "iottdef.h"
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */

GBLREF	int4	gtm_shmflags;	/* Shared memory flags for shmat() */
GBLREF	uint4	gtm_principal_editing_defaults;	/* ext_cap flags if tt */

static nametabent editing_params[] =
{
	{7, "EDITING"},
	{6, "INSERT"},
	{9, "NOEDITING"},
	{8, "NOINSERT"}
};

static unsigned char editing_index[27] =
{
	0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
	2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4
};

/* Unix only environment initializations */
void	gtm_env_init_sp(void)
{
	mstr		val, trans;
	int4		status, index;
	boolean_t	dummy;
	char		buf[MAX_TRANS_NAME_LEN], *token;

	val.addr = GTM_SHMFLAGS;
	val.len = sizeof(GTM_SHMFLAGS) - 1;
	gtm_shmflags = (int4)trans_numeric(&val, &dummy, TRUE);	/* Flags vlaue (0 is undefined or bad) */
	gtm_principal_editing_defaults = 0;
	val.addr = GTM_PRINCIPAL_EDITING;
	val.len = sizeof(GTM_PRINCIPAL_EDITING) - 1;
	if (SS_NORMAL == (status = trans_log_name(&val, &trans, buf)))
	{
		assert(trans.len < sizeof(buf));
		trans.addr[trans.len] = '\0';
		token = strtok(trans.addr, ":");
		while (NULL != token)
		{
			if (ISALPHA(token[0]))
				index = namelook(editing_index, editing_params, token, strlen(token));
			else
				index = -1;	/* ignore this token */
			if (0 <= index)
			{
				switch (index)
				{
				case 0:	/* EDITING */
					gtm_principal_editing_defaults |= TT_EDITING;
					break;
				case 1:	/* INSERT */
					gtm_principal_editing_defaults &= ~TT_NOINSERT;
					break;
				case 2:	/* NOEDITING */
					gtm_principal_editing_defaults &= ~TT_EDITING;
					break;
				case 3:	/* NOINSERT */
					gtm_principal_editing_defaults |= TT_NOINSERT;
					break;
				}
			}
			token = strtok(NULL, ":");
		}
	}
}
