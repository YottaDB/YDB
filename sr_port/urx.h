/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef URX_H_INCLUDED
#define URX_H_INCLUDED

typedef	struct urx_rtnref_type
{
	struct urx_addr_type	*addr;
	struct urx_labref_type	*lab;
	struct urx_rtnref_type	*next;
	unsigned int		len;
	unsigned char		name[1];
} urx_rtnref;

typedef	struct urx_labref_type
{
	struct urx_addr_type	*addr;
	struct urx_labref_type	*next;
	unsigned int		len;
	unsigned char		name[1];
} urx_labref;

/* urx_addr_type and associated prototypes can vary by chip or platform */

#include "urxsp.h"
#include <rtnhdr.h> /* Can be removed when all azl* routines are fixed */

urx_rtnref *urx_putrtn(char *rtn, int rtnlen, urx_rtnref *anchor);
void urx_free(urx_rtnref *anchor);
void urx_add(urx_rtnref *lcl_anchor);
urx_labref **urx_addlab(urx_labref **lp0, urx_labref *lp);
urx_rtnref *urx_addrtn(urx_rtnref *rp_start, urx_rtnref *rp);
bool urx_getlab(char *lab, int lablen, urx_rtnref *rtn, urx_labref **lp0p, urx_labref **lp1p);
bool urx_getrtn(char *rtn, int rtnlen, urx_rtnref **rp0p, urx_rtnref **rp1p, urx_rtnref *anchor);
void urx_remove(rhdtyp *rtn);

#endif /* URX_H_INCLUDED */
