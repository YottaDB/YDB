/****************************************************************
 *								*
 *	Copyright 2008, 2010 Fidelity Information Services, Inc	*
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

STATICFNDCL void zshow_gvstats_output(zshow_out *output, mstr *gldname, mstr *regname, gvstats_rec_t *gvstats);

STATICFNDEF void zshow_gvstats_output(zshow_out *output, mstr *gldname, mstr *regname, gvstats_rec_t *gvstats)
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
	valmstr.len = 0;
	output->flush = TRUE;
	zshow_output(output,&valmstr);
}

void zshow_gvstats(zshow_out *output)
{
	mstr			gldname;
	mstr			regname;
	gvstats_rec_t		cumul_gvstats;	/* statistics for ("*","*") */
	gd_addr			*addr_ptr;
	gd_region		*reg, *r_top;
	sgmnt_addrs		*csa;
	enum db_acc_method	acc_meth;

	memset(&cumul_gvstats, 0, SIZEOF(gvstats_rec_t));
	/* First determine aggregated statistics across ALL <gld,reg> combinations */
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
		{
			if (!reg->open || reg->was_open)
				continue;
			acc_meth = reg->dyn.addr->acc_meth;
			if ((dba_bg != acc_meth) && (dba_mm != acc_meth))
				continue;
			csa = &FILE_INFO(reg)->s_addrs;
#			define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2)	cumul_gvstats.COUNTER += csa->gvstats_rec.COUNTER;
#			include "tab_gvstats_rec.h"
#			undef TAB_GVSTATS_REC
		}
 	}
	cumul_gvstats.db_curr_tn = 0;	/* nullify CTN field as it has no meaning in the "aggregated" sense */
	zshow_gvstats_output(output, &stargldname, &starregname, &cumul_gvstats);
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		get_first_gdr_name(addr_ptr, &gldname);
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
		{
			if (!reg->open)
				continue;
			acc_meth = reg->dyn.addr->acc_meth;
			if ((dba_bg != acc_meth) && (dba_mm != acc_meth))
				continue;
			csa = &FILE_INFO(reg)->s_addrs;
			regname.len = reg->rname_len;
			regname.addr = (char *)&reg->rname[0];
			zshow_gvstats_output(output, &gldname, &regname, &csa->gvstats_rec);
		}
 	}
}
