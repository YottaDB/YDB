/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "copy.h"
#include "cdb_sc.h"
#include "gvcst_protos.h"	/* for gvcst_search_tail,gvcst_search_blk prototype */
#include "min_max.h"
#include "gvcst_expand_key.h"
#include "send_msg.h"
#include "cert_blk.h"

/*
 * -------------------------------------------------------------------
 * Search a single gvcst block
 *
 *	function definition
 *		enum cdb_sc gvcst_search_blk(pKey,pStat)
 *		gv_key *pKey;		- target key
 *		srch_blk_status *pStat;	- status block for this buffer
 *
 *	function returns cdb_sc_normal if successful;  otherwise,
 *	one of the failure codes cdb_sc_badoffset or cdb_sc_blklenerr.
 *
 *	function definition
 *		enum cdb_sc gvcst_search_tail(pKey,pStat,pOldKey)
 *		gv_key *pKey;		- target key
 *		srch_blk_status *pStat;	- status block for this buffer
 *		gv_key *pOldKey;	- key for status block
 *
 *	gvcst_search_tail is identical to gvcst_search_blk,
 *	except instead of starting with the beginning
 *	of the block, it starts where the previous gvcst_search_blk
 *	left off, using the srch_blk_status to set-up.
 *
 *	if successful, fills in srch_blk_status as follows:
 *
 *		---------------------------------
 *		| Block number (untouched)	|
 *		---------------------------------
 *		| Buffer address (input param)	|
 *		---------------------------------
 *		| Transaction number (untouched)|
 *		---------------------------------
 *		| match		| offset	|	previous record
 *		---------------------------------
 *		| match		| offset	|	current record
 *		---------------------------------
 *
 * if the match is not found, or is found at the top or bottom of the
 * block, then the values of the return fields are as follows:
 *
 *				PREVIOUS REC	CURRENT REC
 * CONDITION			MATCH	OFFSET	MATCH	OFFSET
 * ---------			----	------	-----	------
 * Buffer empty			0	0	0	7
 * Hit first key in block	0	0	a	7
 * Hit star key			b	c	0	x
 * Hit last key (leaf)		b	c	a	x
 * Went past last key (leaf)	b	x	0	y
 *
 * where:
 *	a = size of target key
 *	b = number of characters which match on the previous
 *	    key (including those which have been compressed away)
 *	c = offset of previous record
 *	x = offset for last record in the block
 *	y = top of buffer (same as block bsize)
 *
 *
 * Block structure
 *	block : <blk_hdr> <block data>
 *	blk_hdr : <block size> <block level> <transaction number>
 *	blk_data : record...record
 *	record : <rec_hdr> [rec_data]
 *	rec_hdr : <record size> <compression count>
 *	rec_data : [byte...byte]
 * -------------------------------------------------------------------
 */

GBLREF unsigned int	t_tries;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;

error_def(ERR_TEXT);

#ifdef DEBUG
#include "gdscc.h"

#define	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat)				\
{										\
	GBLREF	uint4		dollar_tlevel;					\
										\
	srch_blk_status		*tp_srch_status;				\
	cw_set_element		*cse;						\
										\
	if (dollar_tlevel)							\
	{									\
		tp_srch_status = pStat->first_tp_srch_status;			\
		if (NULL != tp_srch_status)					\
		{								\
			cse = tp_srch_status->cse;				\
			if (NULL != cse)					\
				assert(cse->new_buff == pStat->buffaddr);	\
		}								\
	}									\
}
#else
#define	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat)
#endif

#define	INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat)	if (CDB_STAGNATE <= t_tries) gvcst_search_fail(pStat);

static	void	gvcst_search_fail(srch_blk_status *pStat)
{
	char		buff[1024], crbuff[256], regbuff[512];
	uint4		len;

	assert(CDB_STAGNATE <= t_tries);
	assert((NULL != pStat) && ((NULL != pStat->cr) || (dba_mm == gv_cur_region->dyn.addr->acc_meth)) && (NULL != cs_addrs));
	if (NULL != pStat)
	{
		if (NULL != pStat->cr)
		{
			SPRINTF(crbuff, ": crbuff = 0x%lX", pStat->cr->buffaddr);
			cert_blk(gv_cur_region, pStat->cr->blk, (blk_hdr_ptr_t)GDS_ANY_REL2ABS(cs_addrs, pStat->cr->buffaddr),
				0,  SEND_MSG_ON_CERT_FAIL, NULL);
		} else
			crbuff[0] = '\0';
		len = (6 * SIZEOF(long unsigned int)) + gv_cur_region->rname_len + 1
			+ STRLEN("Possible data corruption in region ")
			+ STRLEN(" : blk = 0x : buff = 0x : cr = 0x%  : csa = 0x% : csalock = 0x%");
		memcpy(regbuff, gv_cur_region->rname, gv_cur_region->rname_len);
		regbuff[gv_cur_region->rname_len] = '\0';
		SNPRINTF(buff, len, "Possible data corruption in region %s : blk = 0x%lX : buff = 0x%lX : cr = 0x%lX %s : "
				"csa = 0x%lX : csalock = 0x%lX",
				regbuff, (long unsigned int)pStat->blk_num, (long unsigned int)pStat->buffaddr,
				(long unsigned int)pStat->cr, crbuff, (long unsigned int)cs_addrs,
				(long unsigned int)cs_addrs->lock_addrs[0]);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));
	}
	assert(t_tries);				/* assert here so we don't have to do it when this returns */
}

#define GVCST_SEARCH_EXPAND_PREVKEY
#define GVCST_SEARCH_BLK
#include "gvcst_blk_search.h" /* for function gvcst_search_blk_expand_prevkey() */ /* BYPASSOK : intentional duplicate include. */
#undef GVCST_SEARCH_BLK
#define GVCST_SEARCH_TAIL
#include "gvcst_blk_search.h" /* for function gvcst_search_tail_expand_prevkey() */ /* BYPASSOK : intentional duplicate include. */
#undef GVCST_SEARCH_TAIL

#undef GVCST_SEARCH_EXPAND_PREVKEY
#define GVCST_SEARCH_BLK
#include "gvcst_blk_search.h" /* for function gvcst_search_blk() */	/* BYPASSOK : intentional duplicate include. */
#undef GVCST_SEARCH_BLK
#define GVCST_SEARCH_TAIL
#include "gvcst_blk_search.h" /* for function gvcst_search_tail() */	/* BYPASSOK : intentional duplicate include. */
#undef GVCST_SEARCH_TAIL
