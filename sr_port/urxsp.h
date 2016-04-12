/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef	struct urx_addr_type
	{
		INTPTR_T			*addr;
		struct urx_addr_type	*next;
	}	urx_addr;

void urx_putlab(char *lab, int lablen, urx_rtnref *rtn, char *addr);
bool azl_geturxlab (char *addr, urx_rtnref *rp);
bool azl_geturxrtn (char *addr, mstr *rname, urx_rtnref **rp);
