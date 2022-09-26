/****************************************************************
 *								*
 * Copyright (c) 2012-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stp_parms.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "copy.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "muextr.h"
#include "iosp.h"
#include "cli.h"
#include "mu_reorg.h"
#include "util.h"
#include "filestruct.h"
#include "error.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "t_qread.h"
#include "tp.h"
#include "mupint.h"
/* Prototypes */
#include "mupip_size.h"
#include "targ_alloc.h"
#include "mupip_exit.h"
#include "gv_select.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "mu_getlst.h"
#include "warn_db_sz.h"
#include "mu_getkey.h"

GBLREF bool			error_mupip;
GBLREF bool			mu_ctrlc_occurred;
GBLREF bool			mu_ctrly_occurred;
GBLREF int			muint_adj;
GBLREF uint4			mu_int_adj[MAX_BT_DEPTH + 1];
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF tp_region		*grlist;
GBLREF unsigned char		rdfail_detail;
GBLREF boolean_t		null_coll_key;
GBLREF gv_key			*mu_start_key;
GBLREF gv_key			*mu_end_key;
GBLREF int			mu_start_keyend;
GBLREF int			mu_end_keyend;

GBLDEF boolean_t mu_subsc	= FALSE;
GBLDEF boolean_t mu_key		= MUKEY_FALSE;
GBLDEF int mu_sub_idx_st	= 0;
GBLDEF int mu_sub_idx_end	= 0;
GBLDEF boolean_t null_coll_key	= FALSE;

typedef struct {
	enum {arsample, scan, impsample}	heuristic;
	int4 					samples;
	int4 					level;
	int4					seed;
} mupip_size_cfg_t;

STATICFNDCL void mupip_size_check_error(void);

/*
 * This function reads command line parameters and forms a configuration for mupip size invocation.
 * It later executes mupip size on each global based on the configuration
 *
 * MUPIP SIZE interface is described in GTM-7292
 */
void mupip_size(void)
{
	boolean_t		restrict_reg = FALSE;
	char 			buff[MAX_LINE], cli_buff[MAX_LINE];
	char 			*p_end;						/* used for strtol validation */
	glist			exclude_gl_head, gl_head, *gl_ptr;
	int4			reg_max_rec, reg_max_key, reg_max_blk;
	mupip_size_cfg_t	mupip_size_cfg = { impsample, 1000, 1, 0 };	/* configuration default values */
	uint4			status = EXIT_NRM;
	unsigned short		BUFF_LEN = SIZEOF(buff), n_len;
	sgmnt_addrs 		*tcsa;
	char 			*db_file_name;
	unsigned char           key_buff[2048];
	unsigned short          keylen;
	boolean_t		subscr = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_outofband_setup();
	error_mupip = FALSE;
	/* Region qualifier */
	grlist = NULL;
	if (CLI_PRESENT == cli_present("REGION"))
	{
		restrict_reg = TRUE;
		gvinit();							/* init gd_header (needed to call mu_getlst) */
		mu_getlst("REGION", SIZEOF(tp_region));
	}
	mupip_size_check_error();
	if (CLI_PRESENT == cli_present("SUBSCRIPT"))
        {
		if (NULL == gv_target)
			gv_target = (gv_namehead *)targ_alloc(DUMMY_GLOBAL_VARIABLE_LEN, NULL, NULL);
		subscr = TRUE;
                keylen = SIZEOF(key_buff);
                if (0 == cli_get_str("SUBSCRIPT", (char *)key_buff, &keylen))
                        mupip_exit(ERR_MUPCLIERR);
                if (FALSE == mu_getkey(key_buff, keylen))
		{
			error_mupip = TRUE;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("SUBSCRIPT"));
		}
		mupip_size_check_error();
		assert(mu_key);
		/* or else "mu_getkey" call above would have returned FALSE */
		/* Create the buff with the global names only to be selected by gv_select below */
		p_end = cli_buff;
		memcpy(p_end ,"^",1);
		p_end++;
		memcpy(p_end , mu_start_key->base, mu_sub_idx_st);
		p_end += mu_sub_idx_st;
		if (mu_sub_idx_end)
		{
			memcpy(p_end ,":^",2);
			p_end += 2;
			memcpy(p_end, mu_end_key->base, mu_sub_idx_end);
			p_end += mu_sub_idx_end;
		}
		*p_end = '\0';
		n_len = p_end - cli_buff;
		gv_target->regcnt--;
		gv_target = NULL;
        }
	/* SELECT qualifier */
	if (!subscr)
	{
		memset(cli_buff, 0, SIZEOF(cli_buff));
		n_len = SIZEOF(cli_buff);
	}
	if ((CLI_PRESENT != cli_present("SELECT")))
	{
		if (!subscr)
		{	/* No select or subscript */
			n_len = 1;
			cli_buff[0] = '*';
		}
	}
	else if (FALSE == cli_get_str("SELECT", cli_buff, &n_len))
	{
		n_len = 1;
		cli_buff[0] = '*';
	}
	/* gv_select will select globals for this clause*/
	gv_select(cli_buff, n_len, FALSE, "SELECT", &gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk, restrict_reg);
	if (!gl_head.next)
	{
		error_mupip = TRUE;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSELECT);
	}
	mupip_size_check_error();
	if (CLI_PRESENT == cli_present("ADJACENCY"))
	{
		assert(SIZEOF(muint_adj) == SIZEOF(int4));
		if (0 == cli_get_int("ADJACENCY", (int4 *)&muint_adj))
		{
			error_mupip = TRUE;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		}
	} else
		muint_adj = DEFAULT_ADJACENCY;
	/* HEURISTIC qualifier */
	if (cli_present("HEURISTIC.SCAN") == CLI_PRESENT)
	{
		mupip_size_cfg.heuristic = scan;
		if (cli_present("HEURISTIC.LEVEL"))
		{
			boolean_t valid = TRUE;
			if (cli_get_str("HEURISTIC.LEVEL", buff, &BUFF_LEN))
			{
				mupip_size_cfg.level = strtol(buff, &p_end, 10);
				valid = (*p_end == '\0');
			}
			else
				valid = FALSE;
			if (!valid || mupip_size_cfg.level <= -MAX_BT_DEPTH || MAX_BT_DEPTH <= mupip_size_cfg.level)
			{
				error_mupip = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.LEVEL"));
			}
		}
		/* else level is already initialized with default value */
	} else if (cli_present("HEURISTIC.ARSAMPLE") == CLI_PRESENT || cli_present("HEURISTIC.IMPSAMPLE") == CLI_PRESENT)
	{
		if (cli_present("HEURISTIC.ARSAMPLE") == CLI_PRESENT)
			mupip_size_cfg.heuristic = arsample;
		else if (cli_present("HEURISTIC.IMPSAMPLE") == CLI_PRESENT)
			mupip_size_cfg.heuristic = impsample;
		if (cli_present("HEURISTIC.SAMPLES"))
		{
			boolean_t valid = cli_get_int("HEURISTIC.SAMPLES", &(mupip_size_cfg.samples));
			if (!valid || mupip_size_cfg.samples <= 0){
				error_mupip = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.SAMPLES"));
			}
		}
		/* else samples is already initialized with default value */
		/* undocumented SEED parameter used for testing sampling method */
		if (cli_present("HEURISTIC.SEED"))
		{
			boolean_t valid = cli_get_int("HEURISTIC.SEED", &(mupip_size_cfg.seed));
			if (!valid){
				error_mupip = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.SEED"));
			}
		}
		/* else seed will be based on the time */
	}
	mupip_size_check_error();
	/* run mupip size on each global */
	for (gl_ptr = gl_head.next; gl_ptr; gl_ptr = gl_ptr->next)
	{
		util_out_print("!/Global: !AD (region !AD)", FLUSH,
			GNAME(gl_ptr).len, GNAME(gl_ptr).addr, REG_LEN_STR(gl_ptr->reg));
		switch (mupip_size_cfg.heuristic)
		{
		case scan:
			status |= mu_size_scan(gl_ptr, mupip_size_cfg.level);
			break;
		case arsample:
			status |= mu_size_arsample(gl_ptr, mupip_size_cfg.samples, mupip_size_cfg.seed);
			break;
		case impsample:
			status |= mu_size_impsample(gl_ptr, mupip_size_cfg.samples, mupip_size_cfg.seed);
			break;
		default:
			assertpro(FALSE && mupip_size_cfg.heuristic);
			break;
		}
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
			mupip_exit(ERR_MUNOFINISH);
	}
	if ((NULL != grlist) && (NULL != grlist->reg))
	{
		tcsa = REG2CSA(grlist->reg);
		if ((NULL != tcsa) && (NULL != tcsa->ti) && (0 != tcsa->ti->total_blks) && (NULL != tcsa->hdr)
			&& (0 !=  MAXTOTALBLKS(tcsa->hdr)))
		{
			db_file_name = (char *)grlist->reg->dyn.addr->fname;
			warn_db_sz(db_file_name, 0, tcsa->ti->total_blks, MAXTOTALBLKS(tcsa->hdr));
		}
	}
	mupip_exit(status ==  EXIT_NRM ? SS_NORMAL : ERR_MUNOFINISH);
}

STATICDEF void mupip_size_check_error(void)
{
	if (error_mupip)
	{
		util_out_print("!/MUPIP SIZE cannot proceed with above errors!/", FLUSH);
		mupip_exit(ERR_MUNOACTION);
	}
}

 /* Performs a random traversal for the sampling methods */
enum cdb_sc mu_size_rand_traverse(double *r, double *a)
{
	boolean_t			first_key = TRUE;
	boolean_t			musz_range_done, is_mm;
	block_id			nBlkId, valBlk[MAX_RECS_PER_BLK]; /* valBlk[j] := value in j-th record of current block */
	cache_rec_ptr_t			cr;
	enum cdb_sc			status;
	int				blk_size, buff_length, cycle, key_size, name_len, rec_len;
	int4				cmp_key, musz_rec, random, rCnt;	/* rCnt : number of entries in valBlk */
	register gv_namehead		*pTarg;
	register srch_blk_status	*pCurr;
	register srch_hist		*pTargHist;
	sm_uc_ptr_t			pVal, pTop, pRec, pBlkBase;
	trans_num			tn;
	unsigned char			nLevl;
	unsigned short			nRecLen;
	unsigned char			buff[MAX_KEY_SZ + 1];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	is_mm = (dba_mm == cs_data->acc_meth);
	pTarg = gv_target;
	pTargHist = &gv_target->hist;
	/* The following largely mimics gvcst_search/gvcst_search_blk */
	nBlkId = pTarg->root;
	tn = cs_addrs->ti->curr_tn;
	if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&cycle, &cr)))
		return (enum cdb_sc)rdfail_detail;
	nLevl = ((blk_hdr_ptr_t)pBlkBase)->levl;
	if (MAX_BT_DEPTH <= (int)nLevl)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_maxlvl;
	}
	if (0 == (int)nLevl)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_badlvl;
	}
	pTargHist->depth = (int)nLevl;
	pCurr = &pTargHist->h[nLevl];
	(pCurr + 1)->blk_num = 0;
	pCurr->tn = tn;
	pCurr->cycle = cycle;
	pCurr->cr = cr;
	for (;;)
	{
		musz_range_done = FALSE;
		musz_rec = 0;
		assert(pCurr->level == nLevl);
		pCurr->cse = NULL;
		pCurr->blk_num = nBlkId;
		pCurr->buffaddr = pBlkBase;
		BLK_LOOP(rCnt, pRec, pBlkBase, pTop, nRecLen, musz_range_done)
		{	/* enumerate records in block */
			GET_AND_CHECK_RECLEN(status, nRecLen, pRec, pTop, nBlkId);
			RETURN_IF_ABNORMAL_STATUS(status);
			CHECK_ADJACENCY(nBlkId, nLevl, a[nLevl]);
			if (mu_subsc) /* Subscript option chosen */
			{
				if ((((rec_hdr *)pRec)->rsiz) != BSTAR_REC_SIZE) /* Did not find the star key */
				{
					GET_KEY_CPY_BUFF(pRec, nRecLen, buff, status);
					RETURN_IF_ABNORMAL_STATUS(status);
					cmp_key = memcmp(buff, mu_start_key->base, mu_start_key->end + 1);
					if (mu_end_key)
					{
						if (0 < memcmp(buff, mu_end_key->base, mu_end_key->end + 1))
							musz_range_done = TRUE;
					} else if (0 < cmp_key)
						musz_range_done = TRUE;
					if (0 > cmp_key)
						continue;
				}
				valBlk[musz_rec] = nBlkId;
				musz_rec++;
			} else
				valBlk[rCnt] = nBlkId;
		}
		(mu_subsc) ? (r[nLevl] = musz_rec) : (r[nLevl] = rCnt);
		/* randomly select next block */
		random = ((mu_subsc) ? (int4)(musz_rec * drand48()) : (int4)(rCnt * drand48()));
		random = random & 0x7fffffff; /* to make sure that the sign bit(msb) is off */
		nBlkId = valBlk[random];
		if (is_mm && (nBlkId > cs_addrs->total_blks))
		{
			if (cs_addrs->total_blks < cs_addrs->ti->total_blks)
				return cdb_sc_helpedout;
			else
				return cdb_sc_blknumerr;
		}
		--pCurr; --nLevl;
		if (nLevl < 1)
			break;
		pCurr->tn = cs_addrs->ti->curr_tn;
		if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&pCurr->cycle, &pCurr->cr)))
			return (enum cdb_sc)rdfail_detail;
		if (((blk_hdr_ptr_t)pBlkBase)->levl != nLevl)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_badlvl;
		}
	}
	return cdb_sc_normal;
}
