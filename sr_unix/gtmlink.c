/****************************************************************
 *								*
 * Copyright (c) 2013-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_caseconv.h"
#include "gtm_string.h"
#include "gtmlink.h"

LITDEF mstr relink_allowed_mstr[] = {
	{0, LEN_AND_LIT("NORECURSIVE")},
	{0, LEN_AND_LIT("RECURSIVE")},		/* if env var $gtm_link = "RECURSIVE", then recursive relink is enabled */
	{0, LEN_AND_LIT("")}
};

#define MAX_KEYWORD_LEN	16

void init_relink_allowed(mstr *keyword)
{
	mstr	trans;
	char	buf[MAX_KEYWORD_LEN];
	int	i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(check_max_keyword_len());
	if ((keyword->len >= MAX_KEYWORD_LEN) || (0 == keyword->len))
	{	/* unrecognized keyword - use default */
		TREF(relink_allowed) = LINK_NORECURSIVE;
		return;
	}
	lower_to_upper((unsigned char *)&buf[0], (unsigned char *)keyword->addr, keyword->len);
	trans.len = keyword->len;
	trans.addr = &buf[0];
	for (i = 0; i < LINK_MAXTYPE; i++)
	{
		if (MSTR_EQ(&trans, &relink_allowed_mstr[i]))
		{
			TREF(relink_allowed) = i;
			return;
		}
	}
	/* unrecognized keyword - use default */
	TREF(relink_allowed) = LINK_NORECURSIVE;
	return;
}

#ifdef DEBUG
void check_max_keyword_len(void)
{
	int	i;

	for (i = 0; i < LINK_MAXTYPE; i++)
		assert(MAX_KEYWORD_LEN > relink_allowed_mstr[i].len);
}
#endif
