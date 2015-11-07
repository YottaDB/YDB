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
#include <descrip.h>
#include <climsgdef.h>

#include "ladef.h"
/* la_getcli.c: retrieves values from the parsed string and fills the pak
		pattern in.  Fills the array of qualified variables.
   used in	: la_maint.c
*/

#define MAXLNG	0xFFFFFFFF
#define CONST	static readonly

bool la_getcli(int v_arr[], pak *pak_ptr)
/* v_arr - qualified variables		*/
/* pak_ptr - buff for pak pattern	*/
{
	bool		cli, valid;
	char		*cptr, ret[ADDR], *vptr[16];		/* pointers to pak variables	*/
	short		t_short;				/* return string length		*/
	int		cnt0, cnt1;
	int4		cli$present(), cli$getvalue();
	int4		result;
	uint4		cli_status, status;
	varid		var;				/* variable identifier (enum type) */
	CONST char	*iden[16] = { "n", "cs", "l" , "std", "oid", "L"  , "nam", "ver",
				      "x", "t0", "t1", "lid", "sid", "nid", "adr", "com" };
	CONST short	len[16] =   {  1, 2, 1,  3,  3,  1,  3,  3,
				       1, 2, 2,  3,  3,  3,  3,  3};
	CONST vtyp	type[16] =  { sho, csm, sho, dat, str, sho, str,  str,
				      sho, dat, dat, lon, mdl, lon, list, str };
	$DESCRIPTOR	(dent, iden);			/* CLI entity descriptor	*/
	$DESCRIPTOR	(dret, ret);			/* CLI return value descrip	*/

	dret.dsc$w_length = ADDR;
	vptr[v_n] = &(pak_ptr->ph.n);
	vptr[v_cs] = pak_ptr->ph.cs;
	vptr[v_l] = pak_ptr->ph.l;
	vptr[v_std] = pak_ptr->pf.std;
	vptr[v_oid] = pak_ptr->pf.oid;
	vptr[v_L] = &(pak_ptr->pd.L);
	vptr[v_nam] = pak_ptr->pd.nam;
	vptr[v_ver] = pak_ptr->pd.ver;
	vptr[v_x] = &(pak_ptr->pd.x);
	vptr[v_t0] = pak_ptr->pd.t0;
	vptr[v_t1] = pak_ptr->pd.t1;
	vptr[v_lid] = &(pak_ptr->pd.lid);
	vptr[v_sid] = (char *)pak_ptr + SIZEOF(pak);
	vptr[v_nid] = vptr[v_sid] + SIZEOF(int4);
	cptr = vptr[v_adr] = vptr[v_nid] + SIZEOF(int4);
	vptr[v_com] = vptr[v_adr] + 4 * ADDR + 4;
	for (cnt0 = 0;  4 != cnt0;  cnt0++)
		*(cptr++) = 0;
	cli = FALSE;
	for (var = v_cs;  var != eovar;  var++)
	{
		dent.dsc$a_pointer= iden[var];
		dent.dsc$w_length = len[var];
		cli_status = cli$present(&dent);
		if (CLI$_PRESENT == cli_status)
		{
			cli = TRUE;
			status = SS$_NORMAL;
			dret.dsc$w_length = ADDR;
			cli_status = cli$get_value(&dent, &dret, &t_short);
			dret.dsc$w_length = t_short;
			ret[t_short] = 0;
			switch (type[var])
			{
			case str:	memcpy(vptr[var], ret, t_short + 1);
					break;
			case list:	memcpy(vptr[var], ret, t_short + 1);
					for (cptr = vptr[var] + t_short + 1, cnt0 = 1;  (CLI$_COMMA == cli_status) && (5 != cnt0);
						cptr += t_short + 1, cnt0++)
					{
						dret.dsc$w_length = ADDR;
						cli_status = cli$get_value(&dent, &dret, &t_short);
						dret.dsc$w_length = t_short;
						ret[t_short] = 0;
						memcpy(cptr, ret, t_short + 1);
					}
					for (;  cnt0 < 5;  cnt0++)
						*(cptr++) = 0;
					break;
			case sho:	status = lib$cvt_dtb((int4)t_short, ret, &result);
					*((short *)vptr[var]) = result;
					break;
			case lon:	status = lib$cvt_dtb((int4)t_short, ret, vptr[var]);
					break;
			case hex:	status = lib$cvt_htb((int4)t_short, ret, vptr[var]);
					break;
			case dat:	status = lib$convert_date_string(&dret, (date *)vptr[var]);
					break;
			case mdl:	valid = la_nam2mdl(vptr[var], t_short, ret);
					if (!valid)
						*((int4 *)vptr[var]) = MAXLNG;
					break;
			case csm:	if (('0' <= ret[0]) && ('9' >= ret[0]))
					{
						status = lib$cvt_dtb(1, ret, &result);
						*((short *)vptr[v_n]) = result;
						cnt0 = v_arr[v_n] = 1;
					} else
						cnt0 = 0;
					for (cnt1 = 0;  (16 != cnt1) && (cnt0 != t_short);  cnt0++)
					{
						if ('-' != ret[cnt0])
						{
							*(vptr[var] + cnt1) = ret[cnt0];
							cnt1++;
						}
					}
					*(vptr[var] + cnt1) = 0;
					break;
			otherwise:	break;
			}
			if (SS$_NORMAL != status)
				la_putmsgs(status);
			v_arr[var] = 1;
		} else
			v_arr[var] = 0;
	}
	pak_ptr->pd.L = 1;		/* only one system allowed for matching */
	return cli;
}
