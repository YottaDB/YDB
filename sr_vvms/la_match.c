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
#include <strdef.h>
#include <descrip.h>

#include "gtm_string.h"
#include "ladef.h"
#include "gtm_caseconv.h"

/* la_match.c:	for two paks pak_ptr0, pak_ptr1 and array of qualified variables computes
		true of "pak pak_ptr0 matches the pattern pak_ptr1" on qualified variables
   used in   :	la_maint.c
*/
#define point(a, b) {	a[v_n] = &(b->ph.n); a[v_cs] = b->ph.cs; a[v_l] = b->ph.l;\
			a[v_std] = &(b->pf.std[1]); a[v_oid] = b->pf.oid; a[v_L] = &(b->pd.L);\
			a[v_nam] = b->pd.nam; a[v_ver] = b->pd.ver; a[v_x] =   &(b->pd.x);\
			a[v_t0] = &(b->pd.t0[1]); a[v_t1] = &(b->pd.t1[1]); a[v_lid] = &(b->pd.lid);\
			a[v_sid] = (char *)b + b->ph.l[3]; \
			a[v_nid] = (char *)b + b->ph.l[4]; \
			a[v_adr] = (char *)b + b->ph.l[5]; \
			a[v_com] = (char *)b + b->ph.l[6]; }

bool la_match(pak *pak_ptr0, pak *pak_ptr1, int v_arr[])
/* pak_ptr0 - pak		*/
/* pak_ptr1 - pak pattern	*/
/* v_arr - qualified variables	*/
{
	bool	match;
	char	*padr;			/* address lines		*/
	char	*ppt[16];		/* pointers to pak variables	*/
	char	*qpt[16];		/* pointers to pattern var	*/
	char	*can;			/* string to match		*/
	char	*pat = NULL;		/* match pattern		*/
	char	buf[128];
	short	cnt;
	int4	str$match_wild();
	int4	status;
	varid	var;			/* variable ident (enum type)	*/
	VARTYP	(type);			/* variable type - ladef	*/
	$DESCRIPTOR(dpat, pat);
	$DESCRIPTOR(dbuf, buf);

	point(ppt, pak_ptr0);
	point(qpt, pak_ptr1);
	var = v_n;
	match = ('0' != pak_ptr0->ph.cs[0]) || (1 == v_arr[v_cs]);
	while ((var != eovar) && match)
	{
		if (1 == v_arr[var])
		{
			switch (type[var])
			{
				case str:	dpat.dsc$a_pointer = qpt[var];
						dpat.dsc$w_length = (short)strlen(qpt[var]);
						dbuf.dsc$w_length = cnt = strlen(ppt[var]);
						lower_to_upper(buf, ppt[var], cnt);
						match = STR$_MATCH == str$match_wild(&dbuf, &dpat);
						break;
				case sho:	match = *(short *)ppt[var] == *(short *)qpt[var];
						break;
				case lon:	match = *(int4 *)ppt[var] == *(int4 *)qpt[var];
						break;
				case hex:	match = *(int4 *)ppt[var] == *(int4 *)qpt[var];
						break;
				case dat:	match = *(uint4 *)ppt[var] >= *(uint4 *)qpt[var] || (0 == *(int4 *)ppt[var]);
						break;
				case lst:	match = FALSE;
						for (cnt = 0;  !match && (cnt < pak_ptr0->pd.L);  cnt++)
						{
							match = *(int4 *)ppt[var] == *(int4 *)qpt[var];
							ppt[var] += SIZEOF(int4);
						}
						break;
				otherwise:	break;
			}
		}
		var++;
	}
	return match;
}
