/****************************************************************
 *								*
 * Copyright (c) 2008-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gvstats_rec.h"
#include "zshow.h"
#include "dpgbldir.h"
#include "gtm_reservedDB.h"

#define	KEYWORD_TERMINATOR	","
#define	KEYWORD_SEPARATOR	":"
#define	GLD_KEYWORD		"GLD" KEYWORD_SEPARATOR
#define	REG_KEYWORD		"REG" KEYWORD_SEPARATOR
#define	STAR_GLD		"*"
#define	STAR_REG		"*"

MSTR_CONST(stargldname, STAR_GLD);
MSTR_CONST(starregname, STAR_REG);
MSTR_CONST(gldkeyword, GLD_KEYWORD);
MSTR_CONST(regkeyword, REG_KEYWORD);
MSTR_CONST(keywordseparator, KEYWORD_SEPARATOR);
MSTR_CONST(keywordterminator, KEYWORD_TERMINATOR);

STATICFNDCL void zshow_gvstats_output(zshow_out *output, mstr *gldname, mstr *regname, gvstats_rec_t *gvstats, boolean_t current);

STATICFNDEF void zshow_gvstats_output(zshow_out *output, mstr *gldname, mstr *regname, gvstats_rec_t *gvstats, boolean_t current)
{
	unsigned char	valstr[MAX_DIGITS_IN_INT8];
	uchar_ptr_t	ptr;
	mstr		valmstr;

	output->flush = FALSE;
	zshow_output(output, &gldkeyword);
	zshow_output(output, gldname);
	zshow_output(output, &keywordterminator);
	zshow_output(output, &regkeyword);
	zshow_output(output, regname);
#	define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2)		\
{								\
	MSTR_CONST(strcounter, TEXT1);				\
	zshow_output(output, &keywordterminator);		\
	zshow_output(output, &strcounter);			\
	zshow_output(output, &keywordseparator);		\
	ptr = i2ascl((uchar_ptr_t)valstr, gvstats->COUNTER);	\
	valmstr.len = (mstr_len_t)(ptr - &valstr[0]);		\
	valmstr.addr = (char *)&valstr[0];			\
	zshow_output(output, &valmstr);				\
}
#	include "tab_gvstats_rec.h"
#	undef TAB_GVSTATS_REC
	if (!current)
	{
		valstr[0] = '?';
		valmstr.addr = (char *)&valstr[0];
		valmstr.len = 1;
	} else
		valmstr.len = 0;
	output->flush = TRUE;
	zshow_output(output, &valmstr);
}

void zshow_gvstats(zshow_out *output, boolean_t total_only)
{
	boolean_t		current;
	enum db_acc_method	acc_meth;
	gd_addr			*addr_ptr;
	gd_region		*reg, *r_top;
	gvstats_rec_t		cumul_gvstats;	/* statistics for ("*","*") */
	mstr			gldname, regname;
	sgmnt_addrs             *csa;

	memset(&cumul_gvstats, 0, SIZEOF(gvstats_rec_t));
	current = TRUE;
	/* First determine aggregated statistics across ALL <gld,reg> combinations */
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
		{
			if (!reg->open || reg->was_open || IS_STATSDB_REG(reg))
				continue;
			acc_meth = reg->dyn.addr->acc_meth;
			if (!IS_ACC_METH_BG_OR_MM(acc_meth))
				continue;
			csa = &FILE_INFO(reg)->s_addrs;
			if ((RDBF_NOSTATS & csa->reservedDBFlags) && !(RDBF_NOSTATS & csa->hdr->reservedDBFlags))
				current = FALSE;
#			define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2) cumul_gvstats.COUNTER += csa->gvstats_rec_p->COUNTER;
#			include "tab_gvstats_rec.h"
#			undef TAB_GVSTATS_REC
		}
 	}
	cumul_gvstats.db_curr_tn = 0;	/* nullify CTN field as it has no meaning in the "aggregated" sense */
	zshow_gvstats_output(output, &stargldname, &starregname, &cumul_gvstats, current);
	if (!total_only)
	{
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			get_first_gdr_name(addr_ptr, &gldname);
			for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
			{
				if (!reg->open || IS_STATSDB_REG(reg))
					continue;
				acc_meth = reg->dyn.addr->acc_meth;
				if (!IS_ACC_METH_BG_OR_MM(acc_meth))
					continue;
				csa = &FILE_INFO(reg)->s_addrs;
				regname.len = reg->rname_len;
				regname.addr = (char *)&reg->rname[0];
				current = !((RDBF_NOSTATS & csa->reservedDBFlags) && !(RDBF_NOSTATS & csa->hdr->reservedDBFlags));
				zshow_gvstats_output(output, &gldname, &regname, csa->gvstats_rec_p, current);
			}
		}
	}
}
